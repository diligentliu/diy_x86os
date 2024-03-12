project(kernel LANGUAGES C)

# 使用自定义的链接器
set(CMAKE_EXE_LINKER_FLAGS "-m elf_i386 -T ${PROJECT_SOURCE_DIR}/kernel.lds")
# set(CMAKE_EXE_LINKER_FLAGS "-m elf_i386 -Ttext=0x100000")
set(CMAKE_C_LINK_EXECUTABLE "${LINKER_TOOL} <OBJECTS> ${CMAKE_EXE_LINKER_FLAGS} -o ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.elf")

# 将所有的汇编、C文件加入工程
# 注意保证start.asm在最前头
file(GLOB_RECURSE C_LIST "*.c" "*.h" "*.S")
add_executable(${PROJECT_NAME} init/start.S ${C_LIST})

# 不带调试信息的elf生成，何种更小，写入到image目录下
add_custom_command(TARGET ${PROJECT_NAME}
                   POST_BUILD
                   COMMAND ${OBJCOPY_TOOL} -S ${PROJECT_NAME}.elf ${CMAKE_SOURCE_DIR}/image/${PROJECT_NAME}.elf
                   COMMAND ${OBJDUMP_TOOL} -x -d -S -m i386 ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.elf > ${PROJECT_NAME}_dis.txt
                   COMMAND ${READELF_TOOL} -a ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.elf > ${PROJECT_NAME}_elf.txt
)