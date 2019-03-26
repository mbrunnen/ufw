if(__UFW_SetupTargetCPU)
  return()
endif()
set(__UFW_SetupTargetCPU 1)

function(add_target_link_flags _target _link_flags)
    set(new_link_flags ${_link_flags})
    get_target_property(existing_link_flags ${_target} LINK_FLAGS)
    if(existing_link_flags)
        set(new_link_flags "${new_link_flags} ${existing_link_flags}")
    else()
        message("No existing link flags found: ${existing_link_flags}")
    endif()
    set_target_properties(${_target} PROPERTIES LINK_FLAGS ${new_link_flags})
endfunction()

function(set_target_cpu target)
  if (NOT PROJECT_TARGET_CPU)
    message(FATAL_ERROR "-- PROJECT_TARGET_CPU is not set! Giving up.")
  elseif (${PROJECT_TARGET_CPU} STREQUAL "c28x-float")
    set(_flags -v28
      --large_memory_model
      --unified_memory
      --float_support=fpu32
      --tmu_support=tmu0
      --define=CPU1
      --define=_TMS320C28XX_TMU0__)
  elseif(${PROJECT_TARGET_CPU} STREQUAL "cortex-m0")
    set(_flags -mthumb -mcpu=cortex-m0)
  elseif(${PROJECT_TARGET_CPU} STREQUAL "cortex-m0+")
    set(_flags -mthumb -mcpu=cortex-m0plus)
  elseif(${PROJECT_TARGET_CPU} STREQUAL "cortex-m1")
    set(_flags -mthumb -mcpu=cortex-m1)
  elseif(${PROJECT_TARGET_CPU} STREQUAL "cortex-m3")
    set(_flags -mthumb -mcpu=cortex-m3)
  elseif(${PROJECT_TARGET_CPU} STREQUAL "cortex-m4")
    set(_flags -mthumb -mcpu=cortex-m4)
  elseif(${PROJECT_TARGET_CPU} STREQUAL "cortex-m4-softfp")
    set(_flags -mthumb -mcpu=cortex-m4 -mfloat-abi=softfp -mfpu=fpv4-sp-d16)
  elseif(${PROJECT_TARGET_CPU} STREQUAL "cortex-m4-hardfp")
    set(_flags -mthumb -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16)
  elseif(${PROJECT_TARGET_CPU} STREQUAL "cortex-m7")
    set(_flags -mthumb -mcpu=cortex-m7)
  elseif(${PROJECT_TARGET_CPU} STREQUAL "cortex-m7-softfp")
    set(_flags -mthumb -mcpu=cortex-m7 -mfloat-abi=softfp -mfpu=fpv5-sp-d16)
  elseif(${PROJECT_TARGET_CPU} STREQUAL "cortex-m7-hardfp")
    set(_flags -mthumb -mcpu=cortex-m7 -mfloat-abi=hard -mfpu=fpv5-sp-d16)
  elseif(${PROJECT_TARGET_CPU} STREQUAL "native")
    return()
  else()
    message(WARNING "-- Unknown PROJECT_TARGET_CPU: ${PROJECT_TARGET_CPU}")
  endif()
  if (PROJECT_TARGET_CPU)
    target_compile_options(${target} PUBLIC ${_flags})
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU" OR
        CMAKE_C_COMPILER_ID STREQUAL "Clang")
        string(REPLACE ";" " " _flags_str "${_flags}")
        set_target_properties(${target} PROPERTIES LINK_FLAGS "${_flags_str}")
    endif()
  endif()
endfunction()