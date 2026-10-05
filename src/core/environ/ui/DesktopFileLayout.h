// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "FileNameLayout.h"
#include "ui/UIText.h"
#include "ui/UIButton.h"
#include "2d/CCLabel.h"

inline void TVPFitFileText(cocos2d::ui::Text *label, const std::string &fullName, float width) {
    label->ignoreContentAdaptWithSize(true);
    label->setTextAreaSize(cocos2d::Size::ZERO);
    const auto fitted = kirikinux::FitFileName(fullName, width, [label](const std::string &value) {
        label->setString(value);
        return label->getVirtualRendererSize().width;
    });
    label->setString(fitted);
}
inline void TVPFitPathButton(cocos2d::ui::Button *button, const std::string &fullName) {
    auto *label = button->getTitleRenderer();
    label->setDimensions(0, 0);
    const auto fitted = kirikinux::FitFileName(fullName,
        std::max(0.f, button->getContentSize().width - 32.f), [button, label](const std::string &value) {
            button->setTitleText(value);
            return label->getContentSize().width;
        });
    button->setTitleText(fitted);
}
