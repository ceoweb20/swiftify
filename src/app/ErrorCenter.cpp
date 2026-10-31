/*
 * Swiftify, a spotify player for desktop
 * Copyright (C) 2026  shred0s
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "app/ErrorCenter.h"
#include "app/Settings.h"
#include "app/Translations.h"

#include <QQmlEngine>

namespace swiftify {

namespace {
ErrorCenter *s_instance = nullptr;
}

ErrorCenter::ErrorCenter(QObject *parent)
    : QObject(parent)
{
    if (!s_instance) {
        s_instance = this;
    }
    if (auto *settings = Settings::instance(); settings && settings->takeRevertedDisabledLanguage()) {
        show(Translations::disabledLanguageErrorMessage());
    }
}

ErrorCenter::~ErrorCenter()
{
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

ErrorCenter *ErrorCenter::instance()
{
    return s_instance;
}

ErrorCenter *ErrorCenter::create(QQmlEngine *, QJSEngine *)
{
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void ErrorCenter::show(const QString &message)
{
    if (message.isEmpty()) {
        clear();
        return;
    }
    m_message = message;
    ++m_serial;
    emit messageChanged();
}

void ErrorCenter::clear()
{
    if (m_message.isEmpty()) {
        return;
    }
    m_message.clear();
    emit messageChanged();
}

} // namespace swiftify
