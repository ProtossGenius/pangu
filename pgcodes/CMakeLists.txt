aux_source_directory(. DIR_LIB_SRCS)
aux_source_directory(./pipeline_detail DIR_LIB_SRCS)
# 生成链接库
add_library(pgcodes ${DIR_LIB_SRCS})

