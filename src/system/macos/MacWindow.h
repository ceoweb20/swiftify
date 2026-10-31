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

class QWindow;

namespace swiftify::macos {

/// takes away what is left of a title bar the content already runs under
/// (Qt::ExpandedClientAreaHint, Qt::NoTitleBarBackgroundHint): the title text
/// and the line beneath it, so only the system's traffic lights stay. the
/// window keeps its rounded corners, shadow and resizable edges.
void styleTitleBar(QWindow *window);

/// keeps two drawables in the window's metal layer rather than the system's
/// three. each is the window's whole size in pixels, 16 MB for a retina
/// window the size of the main one, and the frames swiftify draws are cheap
/// enough that a third, queued behind, buys nothing visible.
/// false if the window has no metal layer yet
bool limitDrawables(QWindow *window);

/// has the window's qt view report its colour space through one that keeps
/// the colour profile at hand. qt asks for the profile twice a frame, to
/// learn whether to draw by the display link, and every time colorsync
/// copied it out afresh into memory mapped for it: a third of the main
/// thread's time while the immersive player drew. it is the window's own
/// colour space all the same, followed to another screen or profile.
/// false if the window has no view or frame to report for yet
bool holdColorSpace(QWindow *window);

} // namespace swiftify::macos
