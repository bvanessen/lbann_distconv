# Add the source files for this directory
set_full_path(THIS_DIR_SOURCES
  batch_normalization.cpp
  dropout.cpp
  entrywise_batch_normalization.cpp
  local_response_normalization.cpp
  selu_dropout.cpp
  )

if (LBANN_HAS_CUDA)
  # Add the CUDA source files for this directory
  set_full_path(THIS_DIR_CU_SOURCES
    batch_normalization.cu
    entrywise_batch_normalization.cu
    )
endif ()

# Propagate the files up the tree
set(SOURCES "${SOURCES}" "${THIS_DIR_SOURCES}" PARENT_SCOPE)
set(CUDA_SOURCES "${CUDA_SOURCES}" "${THIS_DIR_CU_SOURCES}" PARENT_SCOPE)
