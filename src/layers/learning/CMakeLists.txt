# Add the source files for this directory
set_full_path(THIS_DIR_SOURCES
  channelwise_scale_bias.cpp
  convolution.cpp
  deconvolution.cpp
  entrywise_scale_bias.cpp
  embedding.cpp
  fully_connected.cpp
  )

if (LBANN_HAS_CUDA)
  # Add the CUDA source files for this directory
  set_full_path(THIS_DIR_CU_SOURCES
    channelwise_scale_bias.cu
    entrywise_scale_bias.cu
    )
endif ()

# Propagate the files up the tree
set(SOURCES "${SOURCES}" "${THIS_DIR_SOURCES}" PARENT_SCOPE)
set(CUDA_SOURCES "${CUDA_SOURCES}" "${THIS_DIR_CU_SOURCES}" PARENT_SCOPE)
