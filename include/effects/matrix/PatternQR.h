#pragma once

//+--------------------------------------------------------------------------
//
// File:        PatternQR.h
//
// NightDriverStrip - (c) 2018 Plummer's Software LLC.  All Rights Reserved.
//
// This file is part of the NightDriver software project.
//
//    NightDriver is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    NightDriver is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with Nightdriver.  It is normally found in copying.txt
//    If not, see <https://www.gnu.org/licenses/>.
//
//
// Description:
//
//   Displays a QR code that links to the unit's IP
//
// History:     Jul-27-2022         Davepl      Created
//
//---------------------------------------------------------------------------

#ifndef PatternQR_H
#define PatternQR_H

#include "qrcode.h"

#include <memory>

class PatternQR : public EffectWithId<PatternQR>
{
    void construct()
    {
        qrcodeData = std::make_unique<uint8_t[]>(qrcode_getBufferSize(qrVersion));
        lastData = "";
    }

protected:

    String lastData;
    QRCode qrcode;
    std::unique_ptr<uint8_t[]> qrcodeData;
    const int qrVersion = 2;

public:

    PatternQR() : EffectWithId<PatternQR>("QR")
    {
        construct();
    }

    PatternQR(const JsonObjectConst& jsonObject) : EffectWithId<PatternQR>(jsonObject)
    {
        construct();
    }

    virtual ~PatternQR() = default;

    void Start() override
    {
    }

    size_t DesiredFramesPerSecond() const override
    {
        return 20;
    }

    void Draw() override
    {
        String sIP = nd_network::IsWiFiConnected() ? "http://" + nd_network::GetWiFiLocalIP() : "No Wifi";
        if (sIP != lastData)
        {
            lastData = sIP;
            qrcode_initText(&qrcode, qrcodeData.get(), qrVersion, ECC_LOW, sIP.c_str());
        }
        g().fillScreen(g().to16bit(CRGB::DarkBlue));
        const int leftMargin = MATRIX_CENTER_X - qrcode.size / 2;
        const int topMargin = 4;
        const int borderSize = 2;
        const uint16_t foregroundColor = WHITE16;
        const uint16_t borderColor = BLUE16;
        if (qrcode.size + topMargin + borderSize > MATRIX_HEIGHT - 1)
        throw std::runtime_error("Matrix can't hold the QR code height");

        int w = qrcode.size + borderSize * 2;
        int h = w;

        int startX = leftMargin < 0 ? -leftMargin : 0;
        int endX = std::min((int)qrcode.size, MATRIX_WIDTH - leftMargin);

        int startY = topMargin < 0 ? -topMargin : 0;
        int endY = std::min((int)qrcode.size, MATRIX_HEIGHT - topMargin);

        for (uint8_t y = startY; y < endY; y++) {
            for (uint8_t x = startX; x < endX; x++) {
                g().setPixel(leftMargin + x, topMargin + y, (qrcode_getModule(&qrcode, x, y) ? foregroundColor : BLACK16));
            }
        }
    }
};

#endif

