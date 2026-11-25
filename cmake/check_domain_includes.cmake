# Fails if a domain artifact includes a platform artifact
#
# Usage: cmake -DSOURCE_DIR=<project root> -P check_domain_includes.cmake

file(GLOB_RECURSE DOMAIN_FILES
  ${SOURCE_DIR}/semantic_artifacts/include/domain/*.hpp
  ${SOURCE_DIR}/semantic_artifacts/src/domain/*.hpp
  ${SOURCE_DIR}/semantic_artifacts/src/domain/*.cpp
)
if(NOT DOMAIN_FILES)
  message(FATAL_ERROR "No domain artifacts found under ${SOURCE_DIR}/semantic_artifacts")
endif()

foreach(DOMAIN_FILE IN LISTS DOMAIN_FILES)
  file(STRINGS ${DOMAIN_FILE} PLATFORM_INCLUDES REGEX "^[ \t]*#[ \t]*include[ \t]*[<\"]([^>\"]*/)?platform/")
  if(PLATFORM_INCLUDES)
    message(FATAL_ERROR "Domain artifact ${DOMAIN_FILE} includes a platform artifact: ${PLATFORM_INCLUDES}")
  endif()
endforeach()
