
# get git hash
#   get_git_hash(GIT_HASH)           -> full hash
#   get_git_hash(GIT_HASH SHORT)     -> short hash
#   get_git_hash(GIT_HASH SHORT8)    -> first 8 chars of the hash
function(get_git_hash GIT_HASH)
    find_package(Git QUIET)
    if(NOT GIT_EXECUTABLE)
        message(WARNING "Git executable not found")
        return()
    endif()
    
    set(_git_hash_mode "full")
    cmake_parse_arguments(ARG "SHORT;SHORT8" "" "" ${ARGN})


    if(ARG_SHORT)
        set(_git_hash_rev --short HEAD)
        set(_git_hash_mode "short")
    else()
        set(_git_hash_rev HEAD)
    endif()

    execute_process(
       COMMAND git rev-parse ${_git_hash_rev}
       WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
       OUTPUT_VARIABLE _git_hash
       OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(ARG_SHORT8)
        string(SUBSTRING "${_git_hash}" 0 8 _git_hash)
        set(_git_hash_mode "8 chars")
    endif()

    set(${GIT_HASH} "${_git_hash}" PARENT_SCOPE)
    message(STATUS "Git hash (${_git_hash_mode}): ${_git_hash}")
endfunction()

# 获取git当前分支名称
function(get_git_branch GIT_BRANCH)
    find_package(Git QUIET)
    if(NOT GIT_EXECUTABLE)
        message(WARNING "Git executable not found")
        return()
    endif()
    
    execute_process(
       COMMAND git branch --show-current
       WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
       OUTPUT_VARIABLE _git_branch
       OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(${GIT_BRANCH} "${_git_branch}" PARENT_SCOPE)
    message(STATUS "Git branch: ${_git_branch}")
endfunction()

# 获取git当前分支的提交人信息（姓名 <邮箱>）
function(get_git_author GIT_AUTHOR)
    find_package(Git QUIET)
    if(NOT GIT_EXECUTABLE)
        message(WARNING "Git executable not found")
        return()
    endif()

    execute_process(
       COMMAND git log -1 "--format=%an <%ae>"
       WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
       OUTPUT_VARIABLE _git_author
       OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(${GIT_AUTHOR} "${_git_author}" PARENT_SCOPE)
    message(STATUS "Git author: ${_git_author}")
endfunction()
