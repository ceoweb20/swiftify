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

pragma Singleton
pragma ComponentBehavior: Bound
import QtQuick
import Swiftify

QtObject {
    readonly property var locale: Settings.language === "system" ? Qt.locale() : Qt.locale(Settings.language)
    /// "3:07", as playbackTime() writes a song's time
    function playbackTime(milliseconds: real): string {
        const seconds = Math.max(Math.floor(milliseconds / 1000), 0)
        const remainder = seconds % 60
        return Math.floor(seconds / 60) + ":" + (remainder < 10 ? "0" : "") + remainder
    }

    /// "3 hr 24 min", a collection's running time; empty when nothing reports one
    function totalDuration(milliseconds: real): string {
        if (milliseconds <= 0) {
            return ""
        }
        const totalMinutes = Math.floor(milliseconds / 60000)
        const hours = Math.floor(totalMinutes / 60)
        const minutes = totalMinutes % 60
        if (hours > 0) {
            return minutes > 0 ? hours + qsTr(" hr ") + minutes + qsTr(" min") : hours + qsTr(" hr")
        }
        return minutes + qsTr(" min")
    }

    /// "12 songs", "1 song"
    function songCount(count: int): string {
        return count === 1 ? qsTr("%n song", "", count) : qsTr("%n songs", "", count)
    }

    function joined(parts: list<string>, separator: string): string {
        let result = ""
        for (let index = 0; index < parts.length; ++index) {
            const part = parts[index]
            if (part.length === 0) {
                continue
            }
            result += (result.length > 0 ? separator : "") + part
        }
        return result
    }

    /// a size as the mac's file formatter puts it: decimal units, one
    /// decimal place below a hundred
    function fileSize(bytes: real): string {
        if (bytes <= 0) {
            return qsTr("Zero KB")
        }
        if (bytes < 1000) {
            return Math.round(bytes) + qsTr(" bytes")
        }
        const units = [qsTr("KB"), qsTr("MB"), qsTr("GB"), qsTr("TB")]
        let value = bytes / 1000
        let unit = 0
        while (value >= 1000 && unit < units.length - 1) {
            value /= 1000
            unit += 1
        }
        return (value >= 100 ? String(Math.round(value)) : value.toFixed(1)) + " " + units[unit]
    }
}
