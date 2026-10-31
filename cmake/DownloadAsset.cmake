#[[
    Swiftify, a spotify player for desktop
    Copyright (C) 2026  shred0s

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
]]

function(swiftify_download_asset url destination sha256)
    if(EXISTS "${destination}")
        file(SHA256 "${destination}" actual_hash)
        if(actual_hash STREQUAL sha256)
            return()
        endif()
    endif()

    get_filename_component(directory "${destination}" DIRECTORY)
    file(MAKE_DIRECTORY "${directory}")
    set(temporary "${destination}.download")
    message(STATUS "Downloading ${destination}")
    file(DOWNLOAD "${url}" "${temporary}" STATUS download_status TLS_VERIFY ON)
    list(GET download_status 0 download_result)
    list(GET download_status 1 download_message)

    if(download_result AND WIN32)
        find_program(SWIFTIFY_CURL_EXECUTABLE NAMES curl.exe
            HINTS "$ENV{SystemRoot}/System32")
        if(SWIFTIFY_CURL_EXECUTABLE)
            execute_process(
                COMMAND "${SWIFTIFY_CURL_EXECUTABLE}" --fail --location --silent --show-error
                    --output "${temporary}" "${url}"
                RESULT_VARIABLE download_result
                ERROR_VARIABLE download_message)
        endif()
    endif()

    if(NOT "${download_result}" STREQUAL "0")
        file(REMOVE "${temporary}")
        message(FATAL_ERROR "Could not download ${url}: ${download_message}")
    endif()

    file(SHA256 "${temporary}" actual_hash)
    if(NOT actual_hash STREQUAL sha256)
        file(REMOVE "${temporary}")
        message(FATAL_ERROR "SHA256 mismatch for ${url}: expected ${sha256}, got ${actual_hash}")
    endif()
    file(RENAME "${temporary}" "${destination}")
endfunction()
