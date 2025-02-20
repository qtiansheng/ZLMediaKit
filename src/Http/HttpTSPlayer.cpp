﻿/*
 * Copyright (c) 2020 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "HttpTSPlayer.h"

namespace mediakit {

HttpTSPlayer::HttpTSPlayer(const EventPoller::Ptr &poller, bool split_ts) {
    _split_ts = split_ts;
    _segment.setOnSegment([this](const char *data, size_t len) { onPacket(data, len); });
    setPoller(poller ? poller : EventPollerPool::Instance().getPoller());
}

ssize_t HttpTSPlayer::onResponseHeader(const string &status, const HttpClient::HttpHeader &header) {
    _status = status;
    if (status != "200" && status != "206") {
        //http状态码不符合预期
        shutdown(SockException(Err_other, StrPrinter << "bad http status code:" + status));
        return 0;
    }
    auto content_type = const_cast< HttpClient::HttpHeader &>(header)["Content-Type"];
    if (content_type.find("video/mp2t") == 0 || content_type.find("video/mpeg") == 0) {
        _is_ts_content = true;
    }

    //后续是不定长content
    return -1;
}

void HttpTSPlayer::onResponseBody(const char *buf, size_t size, size_t recved_size, size_t total_size) {
    if (_status != "200" && _status != "206") {
        return;
    }
    if (recved_size == size) {
        //开始接收数据
        if (buf[0] == TS_SYNC_BYTE) {
            //这是ts头
            _is_first_packet_ts = true;
        } else {
            WarnL << "可能不是http-ts流";
        }
    }

    if (_split_ts) {
        _segment.input(buf, size);
    } else {
        onPacket(buf, size);
    }
}

void HttpTSPlayer::onResponseCompleted() {
    emitOnComplete(SockException(Err_success, "play completed"));
}

void HttpTSPlayer::onDisconnect(const SockException &ex) {
    emitOnComplete(ex);
}

void HttpTSPlayer::emitOnComplete(const SockException &ex) {
    if (_on_complete) {
        _on_complete(ex);
    }
}

void HttpTSPlayer::onPacket(const char *data, size_t len) {
    if (_on_segment) {
        _on_segment(data, len);
    }
}

void HttpTSPlayer::setOnComplete(onComplete cb) {
    _on_complete = std::move(cb);
}

void HttpTSPlayer::setOnPacket(TSSegment::onSegment cb) {
    _on_segment = std::move(cb);
}

}//namespace mediakit
