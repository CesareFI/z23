# Copyright 2026 Rhett Creighton. Licensed under Apache-2.0.
# Validate actual emitted commands, including each enabled provider compilation.
file(READ "${COMMANDS_FILE}" commands)
string(JSON count LENGTH "${commands}")
if(count EQUAL 0)
    message(FATAL_ERROR "No authored/provider compile commands were checked")
endif()
math(EXPR last "${count} - 1")
set(observed 0)
set(covered 0)
foreach(index RANGE 0 ${last})
    string(JSON source GET "${commands}" ${index} file)
    if(source MATCHES "/native/src/|/vendor/android-[^/]+/|/contexts/commons/packages/")
        string(JSON command GET "${commands}" ${index} command)
        # An earlier enabling flag cannot qualify a command with an opt-out.
        # Inspect actual arguments so quoted flags cannot evade this check,
        # and a macro value containing similar text is not mistaken for a flag.
        separate_arguments(arguments UNIX_COMMAND "${command}")
        set(has_coverage FALSE)
        foreach(argument IN LISTS arguments)
            if(argument MATCHES "^-f(no-sanitize=|sanitize-recover=|no-sanitize-coverage=)")
                message(FATAL_ERROR "Sanitizer or coverage opt-out is forbidden for ${source}: ${argument}")
            endif()
            if(argument MATCHES "^-fsanitize=fuzzer(-no-link)?(,|$)")
                set(has_coverage TRUE)
            endif()
        endforeach()
        list(FIND arguments "-fsanitize=address,undefined" sanitizer_position)
        list(FIND arguments "-fno-sanitize-recover=all" failure_position)
        list(FIND arguments
            "-fsanitize=unsigned-integer-overflow,implicit-integer-truncation,implicit-integer-sign-change"
            integer_position)
        if(sanitizer_position LESS 0 OR failure_position LESS 0)
            message(FATAL_ERROR "Required sanitizer or fail-on-finding flags missing for ${source}")
        endif()
        if(source MATCHES "/native/src/" AND integer_position LESS 0)
            message(FATAL_ERROR "Required authored integer checks missing for ${source}")
        endif()
        # Registered standalone unit tests compile some provider/source copies
        # for fault substitution. Those copies need sanitizers; coverage is
        # required on the libraries and source copies linked into fuzz targets.
        if(NOT command MATCHES "CMakeFiles/[^/ ]+_tests\\.dir/")
            if(NOT has_coverage)
                message(FATAL_ERROR "Required fuzz coverage instrumentation missing for ${source}")
            endif()
            math(EXPR covered "${covered} + 1")
        endif()
        math(EXPR observed "${observed} + 1")
    endif()
endforeach()
if(observed EQUAL 0 OR covered EQUAL 0)
    message(FATAL_ERROR "No authored/provider compile commands were checked")
endif()
message(STATUS "Verified sanitizers/fail-on-finding on ${observed} authored/provider compilations and fuzz coverage on ${covered} library/fuzz compilations")
