add_library(gufo_qwen_dflash STATIC dflash_weights.cpp)
target_include_directories(gufo_qwen_dflash PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(gufo_qwen_dflash PUBLIC gufo_core gufo_qwen_config)
if(ENGINE_ENABLE_HIP)
  target_sources(gufo_qwen_dflash PRIVATE backend.cpp batch.cpp executor.cpp model.cpp)
  target_compile_definitions(gufo_qwen_dflash PUBLIC ENGINE_ENABLE_HIP=1)
  target_link_libraries(gufo_qwen_dflash PUBLIC
    gufo_qwen_control_tokens gufo_qwen_draft_vocab gufo_qwen_hip_common
    gufo_qwen_kernels gufo_qwen_text hip::host roc::hipblas)
endif()
