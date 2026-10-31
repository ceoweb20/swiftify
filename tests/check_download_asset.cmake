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

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/DownloadAsset.cmake")

if(NOT DEFINED TEST_DIRECTORY)
    message(FATAL_ERROR "TEST_DIRECTORY is required")
endif()
set(source "${TEST_DIRECTORY}/source.txt")
set(destination "${TEST_DIRECTORY}/nested/asset.txt")
set(content "download fixture")
string(SHA256 expected_hash "${content}")

if(TEST_MODE STREQUAL "bad_hash")
    swiftify_download_asset("file://${source}" "${destination}.bad_hash" invalid)
    return()
elseif(TEST_MODE STREQUAL "missing_source")
    swiftify_download_asset("file://${source}.missing" "${destination}.missing_source" "${expected_hash}")
    return()
endif()

file(MAKE_DIRECTORY "${TEST_DIRECTORY}")
file(WRITE "${source}" "${content}")
file(REMOVE "${destination}")
swiftify_download_asset("file://${source}" "${destination}" "${expected_hash}")
file(SHA256 "${destination}" actual_hash)
if(NOT actual_hash STREQUAL expected_hash)
    message(FATAL_ERROR "The downloaded file does not match")
endif()

foreach(corruption "" "partial download")
    file(WRITE "${destination}" "${corruption}")
    swiftify_download_asset("file://${source}" "${destination}" "${expected_hash}")
    file(SHA256 "${destination}" actual_hash)
    if(NOT actual_hash STREQUAL expected_hash)
        message(FATAL_ERROR "A corrupt cached file was not repaired")
    endif()
endforeach()

file(REMOVE "${source}")
swiftify_download_asset("file://${source}" "${destination}" "${expected_hash}")
file(WRITE "${source}" "${content}")

foreach(test_mode bad_hash missing_source)
    file(REMOVE "${destination}.${test_mode}" "${destination}.${test_mode}.download")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DTEST_DIRECTORY=${TEST_DIRECTORY}"
        "-DTEST_MODE=${test_mode}" -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(result EQUAL 0)
        message(FATAL_ERROR "${test_mode} unexpectedly succeeded")
    endif()
    if(test_mode STREQUAL "bad_hash" AND NOT error MATCHES "SHA256 mismatch")
        message(FATAL_ERROR "Unexpected hash failure: ${error}")
    elseif(test_mode STREQUAL "missing_source" AND NOT error MATCHES "Could not download")
        message(FATAL_ERROR "Unexpected download failure: ${error}")
    endif()
    if(EXISTS "${destination}.${test_mode}" OR EXISTS "${destination}.${test_mode}.download")
        message(FATAL_ERROR "${test_mode} left a failed download behind")
    endif()
endforeach()
