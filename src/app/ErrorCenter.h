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

#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

namespace swiftify {

/// the one error the app is showing, PlayerModel.errorMessage in the swift
/// app. the banner dismisses itself after a while.
class ErrorCenter : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Errors)
    QML_SINGLETON

    Q_PROPERTY(QString message READ message NOTIFY messageChanged FINAL)
    /// bumped per error, so the same message twice still restarts the banner
    Q_PROPERTY(int serial READ serial NOTIFY messageChanged FINAL)

public:
    /// no default argument: qml builds a singleton with a default constructor
    /// in preference to create(), and would end up with an instance of its own
    explicit ErrorCenter(QObject *parent);
    ~ErrorCenter() override;

    static ErrorCenter *instance();
    static ErrorCenter *create(QQmlEngine *, QJSEngine *);

    QString message() const { return m_message; }
    int serial() const { return m_serial; }

    Q_INVOKABLE void show(const QString &message);
    Q_INVOKABLE void clear();

signals:
    void messageChanged();

private:
    QString m_message;
    int m_serial = 0;
};

} // namespace swiftify
