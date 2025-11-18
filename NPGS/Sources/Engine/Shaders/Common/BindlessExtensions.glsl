#ifndef BINDLESSEXTENSIONS_GLSL_
#define BINDLESSEXTENSIONS_GLSL_

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

struct MyStruct {
	int Member;
};

#endif  // !BINDLESSEXTENSIONS_GLSL_
