#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdio.h>
#include <variant>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <shellscalingapi.h>
#define GL_SILENCE_DEPRECATION
#ifndef GLEW_STATIC
#define GLEW_STATIC
#endif
#include "glew.h"
#include <glfw3.h>
#include <string>
#include <string_view>
#include <memory>
#include <cmath>
#include "filesystem.hpp"
#include "imgui_stdlib.h"
#include "stools.hpp"
#include "texture.hpp"
#include "lunasvg.h"
#include "asvg.hpp"
#include "templateproject.hpp"


namespace template_project {
void project_to_bytes(project const& p, serialization::out_buffer& buffer);
project bytes_to_project(serialization::in_buffer& buffer);
}

GLint compile_shader(std::string_view source, GLenum type) {
	GLuint return_value = glCreateShader(type);

	if(return_value == 0) {
		MessageBoxW(nullptr, L"shader creation failed", L"OpenGL error", MB_OK);
	}

	std::string s_source(source);
	GLchar const* texts[] = {
		"#version 140\r\n",
		"#extension GL_ARB_explicit_uniform_location : enable\r\n",
		"#extension GL_ARB_explicit_attrib_location : enable\r\n",
		"#extension GL_ARB_shader_subroutine : enable\r\n",
		"#define M_PI 3.1415926535897932384626433832795\r\n",
		"#define PI 3.1415926535897932384626433832795\r\n",
		s_source.c_str()
	};
	glShaderSource(return_value, 7, texts, nullptr);
	glCompileShader(return_value);

	GLint result;
	glGetShaderiv(return_value, GL_COMPILE_STATUS, &result);
	if(result == GL_FALSE) {
		GLint log_length = 0;
		glGetShaderiv(return_value, GL_INFO_LOG_LENGTH, &log_length);

		auto log = std::unique_ptr<char[]>(new char[static_cast<size_t>(log_length)]);
		GLsizei written = 0;
		glGetShaderInfoLog(return_value, log_length, &written, log.get());
		auto error = std::string("Shader failed to compile:\n") + log.get();
		MessageBoxA(nullptr, error.c_str(), "OpenGL error", MB_OK);
	}
	return return_value;
}

GLuint create_program(std::string_view vertex_shader, std::string_view fragment_shader) {
	GLuint return_value = glCreateProgram();
	if(return_value == 0) {
		MessageBoxW(nullptr, L"program creation failed", L"OpenGL error", MB_OK);
	}

	auto v_shader = compile_shader(vertex_shader, GL_VERTEX_SHADER);
	auto f_shader = compile_shader(fragment_shader, GL_FRAGMENT_SHADER);

	glAttachShader(return_value, v_shader);
	glAttachShader(return_value, f_shader);
	glLinkProgram(return_value);

	GLint result;
	glGetProgramiv(return_value, GL_LINK_STATUS, &result);
	if(result == GL_FALSE) {
		GLint logLen;
		glGetProgramiv(return_value, GL_INFO_LOG_LENGTH, &logLen);

		char* log = new char[static_cast<size_t>(logLen)];
		GLsizei written;
		glGetProgramInfoLog(return_value, logLen, &written, log);
		auto err = std::string("Program failed to link:\n") + log;
		MessageBoxA(nullptr, err.c_str(), "OpenGL error", MB_OK);
	}

	glDeleteShader(v_shader);
	glDeleteShader(f_shader);

	return return_value;
}

static GLfloat global_square_data[] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f };
static GLfloat global_square_right_data[] = { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f };
static GLfloat global_square_left_data[] = { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f };
static GLfloat global_square_flipped_data[] = { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f };
static GLfloat global_square_right_flipped_data[] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f };
static GLfloat global_square_left_flipped_data[] = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f };

static GLuint ui_shader_program = 0;

void load_shaders() {

	std::string_view fx_str =
		"in vec2 tex_coord;\n"
		"out vec4 frag_color;\n"
		"uniform sampler2D texture_sampler;\n"
		"uniform vec4 d_rect;\n"
		"uniform float border_size;\n"
		"uniform float grid_size;\n"
		"uniform vec2 grid_off;\n"
		"uniform vec3 inner_color;\n"
		"uniform uvec2 subroutines_index;\n"

		"vec4 empty_rect(vec2 tc) {\n"
			"float realx = tc.x * d_rect.z;\n"
			"float realy = tc.y * d_rect.w;\n"
			"if(realx <= 2.5 || realy <= 2.5 || realx >= (d_rect.z -2.5) || realy >= (d_rect.w -2.5))\n"
				"return vec4(inner_color.r, inner_color.g, inner_color.b, 1.0f);\n"
			"return vec4(inner_color.r, inner_color.g, inner_color.b, 0.25f);\n"
		"}\n"
		"vec4 hollow_rect(vec2 tc) {\n"
			"float realx = tc.x * d_rect.z;\n"
			"float realy = tc.y * d_rect.w;\n"
			"if(realx <= 4.5 || realy <= 4.5 || realx >= (d_rect.z -4.5) || realy >= (d_rect.w -4.5))\n"
			"return vec4(inner_color.r, inner_color.g, inner_color.b, 0.25f);\n"
			"return vec4(inner_color.r, inner_color.g, inner_color.b, 0.0f);\n"
		"}\n"
		"vec4 grid_texture(vec2 tc) {\n"
			"float realx = grid_off.x + tc.x * d_rect.z;\n"
			"float realy = grid_off.y + tc.y * d_rect.w;\n"
			"if(mod(realx, grid_size) < 1.0f || mod(realy, grid_size) < 1.0f)\n"
				"return vec4(1.0f, 1.0f, 1.0f, 0.1f);\n"
			"return vec4(0.0f, 0.0f, 0.0f, 0.0f);\n"
		"}\n"
		"vec4 direct_texture(vec2 tc) {\n"
			"float realx = tc.x * d_rect.z;\n"
			"float realy = tc.y * d_rect.w;\n"
//			"if(realx <= 2.5 || realy <= 2.5 || realx >= (d_rect.z -2.5) || realy >= (d_rect.w -2.5))\n"
//				"return vec4(inner_color.r, inner_color.g, inner_color.b, 1.0f);\n"
			"\treturn texture(texture_sampler, tc);\n"
		"}\n"
		"vec4 frame_stretch(vec2 tc) {\n"
			"float realx = tc.x * d_rect.z;\n"
			"float realy = tc.y * d_rect.w;\n"
			"if(realx <= 2.5 || realy <= 2.5 || realx >= (d_rect.z -2.5) || realy >= (d_rect.w -2.5))\n"
				"return vec4(inner_color.r, inner_color.g, inner_color.b, 1.0f);\n"
			"vec2 tsize = textureSize(texture_sampler, 0);\n"
			"float xout = 0.0;\n"
			"float yout = 0.0;\n"
			"if(realx <= border_size * grid_size)\n"
				"xout = realx / (tsize.x * grid_size);\n"
			"else if(realx >= (d_rect.z - border_size * grid_size))\n"
				"xout = (1.0 - border_size / tsize.x) + (border_size * grid_size - (d_rect.z - realx)) / (tsize.x * grid_size);\n"
			"else\n"
				"xout = border_size / tsize.x + (1.0 - 2.0 * border_size / tsize.x) * (realx - border_size * grid_size) / (d_rect.z * 2.0 * border_size * grid_size);\n"
			"if(realy <= border_size * grid_size)\n"
				"yout = realy / (tsize.y * grid_size);\n"
			"else if(realy >= (d_rect.w - border_size * grid_size))\n"
				"yout = (1.0 - border_size / tsize.y) + (border_size * grid_size - (d_rect.w - realy)) / (tsize.y * grid_size);\n"
			"else\n"
				"yout = border_size / tsize.y + (1.0 - 2.0 * border_size / tsize.y) * (realy - border_size * grid_size) / (d_rect.w * 2.0 * border_size * grid_size);\n"
			"return texture(texture_sampler, vec2(xout, yout));\n"
		"}\n"
		"vec4 coloring_function(vec2 tc) {\n"
			"\tswitch(int(subroutines_index.x)) {\n"
				"\tcase 1: return empty_rect(tc);\n"
				"\tcase 2: return direct_texture(tc);\n"
				"\tcase 3: return frame_stretch(tc);\n"
				"\tcase 4: return grid_texture(tc);\n"
				"\tcase 5: return hollow_rect(tc);\n"
				"\tdefault: break;\n"
			"\t}\n"
			"\treturn vec4(1.0f,1.0f,1.0f,1.0f);\n"
		"}\n"
		"void main() {\n"
			"\tfrag_color = coloring_function(tex_coord);\n"
		"}";
	std::string_view vx_str =
		"layout (location = 0) in vec2 vertex_position;\n"
		"layout (location = 1) in vec2 v_tex_coord;\n"
		"out vec2 tex_coord;\n"
		"uniform float screen_width;\n"
		"uniform float screen_height;\n"
		"uniform vec4 d_rect;\n"
		"void main() {\n"
			"\tgl_Position = vec4(\n"
				"\t\t-1.0 + (2.0 * ((vertex_position.x * d_rect.z)  + d_rect.x) / screen_width),\n"
				"\t\t 1.0 - (2.0 * ((vertex_position.y * d_rect.w)  + d_rect.y) / screen_height),\n"
				"\t\t0.0, 1.0);\n"
			"\ttex_coord = v_tex_coord;\n"
		"}";

	ui_shader_program = create_program(vx_str, fx_str);
}

static GLuint global_square_vao = 0;
static GLuint global_square_buffer = 0;
static GLuint global_square_right_buffer = 0;
static GLuint global_square_left_buffer = 0;
static GLuint global_square_flipped_buffer = 0;
static GLuint global_square_right_flipped_buffer = 0;
static GLuint global_square_left_flipped_buffer = 0;

static GLuint sub_square_buffers[64] = { 0 };

void load_global_squares() {
	glGenBuffers(1, &global_square_buffer);

	// Populate the position buffer
	glBindBuffer(GL_ARRAY_BUFFER, global_square_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 16, global_square_data, GL_STATIC_DRAW);

	glGenVertexArrays(1, &global_square_vao);
	glBindVertexArray(global_square_vao);
	glEnableVertexAttribArray(0); // position
	glEnableVertexAttribArray(1); // texture coordinates

	glBindVertexBuffer(0, global_square_buffer, 0, sizeof(GLfloat) * 4);

	glVertexAttribFormat(0, 2, GL_FLOAT, GL_FALSE, 0);					 // position
	glVertexAttribFormat(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 2);	// texture coordinates
	glVertexAttribBinding(0, 0);											// position -> to array zero
	glVertexAttribBinding(1, 0);											 // texture coordinates -> to array zero

	glGenBuffers(1, &global_square_left_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, global_square_left_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 16, global_square_left_data, GL_STATIC_DRAW);

	glGenBuffers(1, &global_square_right_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, global_square_right_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 16, global_square_right_data, GL_STATIC_DRAW);

	glGenBuffers(1, &global_square_right_flipped_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, global_square_right_flipped_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 16, global_square_right_flipped_data, GL_STATIC_DRAW);

	glGenBuffers(1, &global_square_left_flipped_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, global_square_left_flipped_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 16, global_square_left_flipped_data, GL_STATIC_DRAW);

	glGenBuffers(1, &global_square_flipped_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, global_square_flipped_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 16, global_square_flipped_data, GL_STATIC_DRAW);

	glGenBuffers(64, sub_square_buffers);
	for(uint32_t i = 0; i < 64; ++i) {
		glBindBuffer(GL_ARRAY_BUFFER, sub_square_buffers[i]);

		float const cell_x = static_cast<float>(i & 7) / 8.0f;
		float const cell_y = static_cast<float>((i >> 3) & 7) / 8.0f;

		GLfloat global_sub_square_data[] = { 0.0f, 0.0f, cell_x, cell_y, 0.0f, 1.0f, cell_x, cell_y + 1.0f / 8.0f, 1.0f, 1.0f,
			cell_x + 1.0f / 8.0f, cell_y + 1.0f / 8.0f, 1.0f, 0.0f, cell_x + 1.0f / 8.0f, cell_y };

		glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 16, global_sub_square_data, GL_STATIC_DRAW);
	}
}



void render_textured_rect(color3f color, float ix, float iy, int32_t iwidth, int32_t iheight, GLuint texture_handle) {
	if(texture_handle == 0)
		return;

	float x = float(ix);
	float y = float(iy);
	float width = float(iwidth);
	float height = float(iheight);

	glBindVertexArray(global_square_vao);

	glBindVertexBuffer(0, global_square_buffer, 0, sizeof(GLfloat) * 4);

	glUniform4f(glGetUniformLocation(ui_shader_program, "d_rect"), x, y, width, height);
	glUniform3f(glGetUniformLocation(ui_shader_program, "inner_color"), color.r, color.g, color.b);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture_handle);

	GLuint subroutines[2] = { 2, 0 };
	glUniform2ui(glGetUniformLocation(ui_shader_program, "subroutines_index"), subroutines[0], subroutines[1]);
	//glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 2, subroutines); // must set all subroutines in one call

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}
void render_stretch_textured_rect(color3f color, float ix, float iy, float ui_scale, int32_t iwidth, int32_t iheight, float border_size, GLuint texture_handle) {
	float x = float(ix);
	float y = float(iy);
	float width = float(iwidth);
	float height = float(iheight);

	glBindVertexArray(global_square_vao);

	glBindVertexBuffer(0, global_square_buffer, 0, sizeof(GLfloat) * 4);

	glUniform1f(glGetUniformLocation(ui_shader_program, "border_size"), border_size);
	glUniform1f(glGetUniformLocation(ui_shader_program, "grid_size"), ui_scale);
	glUniform4f(glGetUniformLocation(ui_shader_program, "d_rect"), x, y, width, height);
	glUniform3f(glGetUniformLocation(ui_shader_program, "inner_color"), color.r, color.g, color.b);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture_handle);

	GLuint subroutines[2] = { 3, 0 };
	glUniform2ui(glGetUniformLocation(ui_shader_program, "subroutines_index"), subroutines[0], subroutines[1]);
	//glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 2, subroutines); // must set all subroutines in one call

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}
void render_empty_rect(color3f color, float ix, float iy, int32_t iwidth, int32_t iheight) {
	float x = float(ix);
	float y = float(iy);
	float width = float(iwidth);
	float height = float(iheight);

	glBindVertexArray(global_square_vao);

	glBindVertexBuffer(0, global_square_buffer, 0, sizeof(GLfloat) * 4);

	glUniform4f(glGetUniformLocation(ui_shader_program, "d_rect"), x, y, width, height);
	glUniform3f(glGetUniformLocation(ui_shader_program, "inner_color"), color.r, color.g, color.b);

	GLuint subroutines[2] = { 1, 0 };
	glUniform2ui(glGetUniformLocation(ui_shader_program, "subroutines_index"), subroutines[0], subroutines[1]);
	//glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 2, subroutines); // must set all subroutines in one call

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}
void render_hollow_rect(color3f color, float ix, float iy, int32_t iwidth, int32_t iheight) {
	float x = float(ix);
	float y = float(iy);
	float width = float(iwidth);
	float height = float(iheight);

	glBindVertexArray(global_square_vao);

	glBindVertexBuffer(0, global_square_buffer, 0, sizeof(GLfloat) * 4);

	glUniform4f(glGetUniformLocation(ui_shader_program, "d_rect"), x, y, width, height);
	glUniform3f(glGetUniformLocation(ui_shader_program, "inner_color"), color.r, color.g, color.b);

	GLuint subroutines[2] = { 5, 0 };
	glUniform2ui(glGetUniformLocation(ui_shader_program, "subroutines_index"), subroutines[0], subroutines[1]);
	//glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 2, subroutines); // must set all subroutines in one call

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void render_layout_rect(color3f outline_color, float ix, float iy, int32_t iwidth, int32_t iheight) {
	render_empty_rect(outline_color * 0.5f, ix, iy, iwidth, iheight);
	render_hollow_rect(outline_color, ix, iy, iwidth, iheight);
}

static void glfw_error_callback(int error, const char* description) {
	MessageBoxW(nullptr, L"GLFW Error", L"OpenGL error", MB_OK);
}

enum class drag_target {
	none, center, left, right, top, bottom, top_left, top_right, bottom_left, bottom_right
};


std::wstring relative_file_name(std::wstring_view base_name, std::wstring_view project_directory) {
	if(base_name.length() > 1) {
		size_t common_length = 0;
		while(common_length < base_name.size()) {
			auto next_common_length = base_name.find_first_of(L'\\', common_length);
			if (next_common_length != std::string::npos) {
				next_common_length++;
			}
			if(base_name.substr(0, next_common_length) != project_directory.substr(0, next_common_length)) {
				break;
			}
			common_length = next_common_length;
		}
		uint32_t missing_separators_count = 0;
		for(size_t i = common_length; i < project_directory.size(); ++i) {
			if(project_directory[i] == L'\\') {
				++missing_separators_count;
			}
		}
		if(missing_separators_count == 0) {
			if(common_length >= base_name.size()) {
				auto last_sep = base_name.find_last_of(L'\\');
				if(last_sep == std::wstring::npos)
					return std::wstring(base_name);

				return std::wstring(base_name.substr(last_sep + 1));
			} else {
				return std::wstring(base_name.substr(common_length));
			}
		} else {
			std::wstring temp;
			for(uint32_t i = 0; i < missing_separators_count; ++i) {
				temp += L"..\\";
			}
			if(common_length >= base_name.size()) {
				std::abort(); // impossible
				//return temp;
			} else {
				return temp + std::wstring(base_name.substr(common_length));
			}
		}
	}
	return std::wstring(base_name);
}
drag_target test_rect_target(float pos_x, float pos_y, float rx, float ry, float rw, float rh, float scale) {
	bool top = false;
	bool bottom = false;
	bool left = false;
	bool right = false;
	if(rx - 2.0f * scale <= pos_x && pos_x <= rx + 2.0f * scale && ry - 2.0f * scale <= pos_y && pos_y <= ry + rh + 2.0f * scale)
		left = true;
	if(ry - 2.0f * scale <= pos_y && pos_y <= ry + 2.0f * scale && rx - 2.0f * scale <= pos_x && pos_x <= rx + rw + 2.0f * scale)
		top = true;
	if(rx + rw - 2.0f * scale <= pos_x && pos_x <= rx + rw + 2.0f * scale && ry - 2.0f * scale <= pos_y && pos_y <= ry + rh + 2.0f * scale)
		right = true;
	if(ry + rh - 2.0f * scale <= pos_y && pos_y <= ry + rh + 2.0f * scale && rx  - 2.0f * scale <= pos_x && pos_x <= rx + rw + 2.0f * scale)
		bottom = true;
	if(top && bottom)
		return drag_target::bottom;
	if(left && right)
		return drag_target::right;
	if(top && left)
		return drag_target::top_left;
	if(top && right)
		return drag_target::top_right;
	if(bottom && left)
		return drag_target::bottom_left;
	if(bottom && right)
		return drag_target::bottom_right;
	if(left)
		return drag_target::left;
	if(right)
		return drag_target::right;
	if(top)
		return drag_target::top;
	if(bottom)
		return drag_target::bottom;
	if(rx <= pos_x && pos_x <= rx + rw && ry <= pos_y && pos_y <= ry + rh)
		return drag_target::center;
	return drag_target::none;
}

void mouse_to_drag_type(drag_target t) {
	switch(t) {
		case drag_target::none: break;
		case drag_target::center: ImGui::SetMouseCursor(ImGuiMouseCursor_::ImGuiMouseCursor_ResizeAll); break;
		case drag_target::left: ImGui::SetMouseCursor(ImGuiMouseCursor_::ImGuiMouseCursor_ResizeEW); break;
		case drag_target::right: ImGui::SetMouseCursor(ImGuiMouseCursor_::ImGuiMouseCursor_ResizeEW); break;
		case drag_target::top: ImGui::SetMouseCursor(ImGuiMouseCursor_::ImGuiMouseCursor_ResizeNS); break;
		case drag_target::bottom: ImGui::SetMouseCursor(ImGuiMouseCursor_::ImGuiMouseCursor_ResizeNS); break;
		case drag_target::top_left: ImGui::SetMouseCursor(ImGuiMouseCursor_::ImGuiMouseCursor_ResizeNWSE); break;
		case drag_target::top_right: ImGui::SetMouseCursor(ImGuiMouseCursor_::ImGuiMouseCursor_ResizeNESW); break;
		case drag_target::bottom_left: ImGui::SetMouseCursor(ImGuiMouseCursor_::ImGuiMouseCursor_ResizeNESW); break;
		case drag_target::bottom_right: ImGui::SetMouseCursor(ImGuiMouseCursor_::ImGuiMouseCursor_ResizeNWSE); break;
		default: break;
	}

}

float last_scroll_value = 0.0f;
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
	last_scroll_value += float(yoffset);
}

float drag_offset_x = 0.0f;
float drag_offset_y = 0.0f;

bool paths_are_the_same(std::vector<size_t>& left, std::vector<size_t>& right) {
	if (left.size() != right.size()) return false;
	for (size_t i = 0; i < left.size(); i++) {
		if (left[i] != right[i]) {
			return false;
		}
	}
	return true;
}

template<size_t SIZE>
bool paths_are_the_same(std::array<size_t, SIZE>& left, size_t left_size, std::array<size_t, SIZE>& right, size_t right_size) {
	if (left_size != right_size) return false;
	for (size_t i = 0; i < left_size; i++) {
		if (left[i] != right[i]) {
			return false;
		}
	}
	return true;
}

bool paths_are_the_same(std::vector<size_t>& left, size_t left_attachment, std::vector<size_t>& right) {
	if (left.size() + 1 != right.size()) return false;
	for (size_t i = 0; i < left.size(); i++) {
		if (left[i] != right[i]) {
			return false;
		}
	}
	if (left_attachment != right.back()) {
		return false;
	}
	return true;
}



auto base_tree_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DrawLinesFull;

template_project::project open_project;
template_project::template_type selected_type = template_project::template_type::background;
int32_t selected_template = -1;

template<typename F>
void make_name_change(std::string& temp_name, std::string& real_name, std::vector<F> const& options) {
	ImGui::InputText("Name", &temp_name);
	if(!ImGui::IsItemActiveAsInputText()) {
		if(temp_name == real_name) {
			// nothing; nothing has changed
		} else if(temp_name.empty()) {
			temp_name = real_name;
		} else {
			if(std::find_if(options.begin(), options.end(), [&](auto const& opt) { return opt.display_name == temp_name; }) != options.end()) {
				MessageBoxW(nullptr, L"Name must be unique", L"Invalid Name", MB_OK);
			} else {
				real_name = temp_name;
			}
		}
	}
}
void make_color_combo_box(int32_t& color_choice, char const* label, template_project::project const& current_theme) {
	ImGui::PushID((void*)&color_choice);
	
	int32_t combo_selection = color_choice + 1;
	std::vector<char const*> options;
	options.push_back("--None--");

	for(auto& c : current_theme.colors) {
		options.push_back(c.display_name.c_str());
	}
	
	if(ImGui::Combo(label, &combo_selection, options.data(), int32_t(options.size()))) {
		color_choice = combo_selection - 1;
	}
	ImGui::PopID();
}
void make_background_combo_box(int32_t& bg_choice, char const* label, template_project::project const& current_theme) {
	ImGui::PushID((void*)&bg_choice);

	int32_t combo_selection = bg_choice + 1;
	std::vector<char const*> options;
	options.push_back("--None--");

	for(auto& c : current_theme.backgrounds) {
		options.push_back(c.file_name.c_str());
	}

	if(ImGui::Combo(label, &combo_selection, options.data(), int32_t(options.size()))) {
		bg_choice = combo_selection - 1;
	}
	ImGui::PopID();
}
void make_icon_combo_box(int32_t& icon_choice, char const* label, template_project::project const& current_theme) {
	ImGui::PushID((void*)&icon_choice);

	int32_t combo_selection = icon_choice + 1;
	std::vector<char const*> options;
	options.push_back("--None--");

	for(auto& c : current_theme.icons) {
		options.push_back(c.file_name.c_str());
	}

	if(ImGui::Combo(label, &combo_selection, options.data(), int32_t(options.size()))) {
		icon_choice = combo_selection - 1;
	}
	ImGui::PopID();
}
void make_icon_button_combo_box(int32_t& icon_choice, char const* label, template_project::project const& current_theme) {
	ImGui::PushID((void*)&icon_choice);

	int32_t combo_selection = icon_choice + 1;
	std::vector<char const*> options;
	options.push_back("--None--");

	for(auto& c : current_theme.iconic_button_t) {
		options.push_back(c.display_name.c_str());
	}

	if(ImGui::Combo(label, &combo_selection, options.data(), int32_t(options.size()))) {
		icon_choice = combo_selection - 1;
	}
	ImGui::PopID();
}
void make_layout_region_combo_box(int32_t& region_choice, char const* label, template_project::project const& current_theme) {
	ImGui::PushID((void*)&region_choice);

	int32_t combo_selection = region_choice + 1;
	std::vector<char const*> options;
	options.push_back("--None--");

	for(auto& c : current_theme.layout_region_t) {
		options.push_back(c.display_name.c_str());
	}

	if(ImGui::Combo(label, &combo_selection, options.data(), int32_t(options.size()))) {
		region_choice = combo_selection - 1;
	}
	ImGui::PopID();
}
void make_font_combo_box(int32_t& font_choice, char const* label, template_project::project const& current_theme) {
	ImGui::PushID((void*)&font_choice);

	int32_t combo_selection = font_choice;
	std::vector<char const*> options;
	//options.push_back("--None--");
	options.push_back("Body font");
	options.push_back("Header font");

	//for(auto& c : current_theme.icons) {
	//	options.push_back(c.file_name.c_str());
	//}

	if(ImGui::Combo(label, &combo_selection, options.data(), int32_t(options.size()))) {
		font_choice = combo_selection;
	}
	ImGui::PopID();
}
void make_text_region_description(template_project::text_region_template& tt, char const* label, template_project::project& current_theme) {
	ImGui::PushID((void*)&tt);
	ImGui::Text(label);
	ImGui::Indent(20.0f);
	make_background_combo_box(tt.bg, "Background", current_theme);
	make_color_combo_box(tt.text_color, "Text color", current_theme);
	make_font_combo_box(tt.font_choice, "Text font", current_theme);
	ImGui::InputFloat("Text scale", &tt.font_scale);
	ImGui::InputFloat("H. margins (in grid units)", &tt.h_text_margins);
	ImGui::InputFloat("V. margins (in grid units)", &tt.v_text_margins);
	{
		int32_t combo_selection = int32_t(tt.h_text_alignment);
		const char* new_template_options[] = { "left", "right", "center" };
		if(ImGui::Combo("H. alignment", &combo_selection, new_template_options, 3)) {
			tt.h_text_alignment = template_project::aui_text_alignment(combo_selection);
		}
	}
	{
		int32_t combo_selection = int32_t(tt.v_text_alignment);
		const char* new_template_options[] = { "top", "bottom", "center" };
		if(ImGui::Combo("V. alignment", &combo_selection, new_template_options, 3)) {
			tt.v_text_alignment = template_project::aui_text_alignment(combo_selection);
		}
	}
	ImGui::Unindent(20.0f);
	ImGui::PopID();
}
void make_affine_transform_description(template_project::affine_transform& tf, char const* label, template_project::project& current_theme) {
	ImGui::PushID((void*)&tf);
	ImGui::Text(label);
	ImGui::Indent(20.0f);

	int32_t combo_selection = int32_t(tf.dimension);
	const char* new_template_options[] = { "height", "width", "smallest dimension", "largest dimension", "diagonal" };
	if(ImGui::Combo("Base value", &combo_selection, new_template_options, 5)) {
		tf.dimension = template_project::dimension_relative(combo_selection);
	}
	ImGui::InputFloat("Multiplier", &tf.scale);
	ImGui::InputFloat("Offset (in grid units)", &tf.offset);
	
	ImGui::Unindent(20.0f);
	ImGui::PopID();
}
void make_icon_region_description(template_project::icon_region_template& tt, char const* label, template_project::project& current_theme) {
	ImGui::PushID((void*)&tt);
	ImGui::Text(label);
	ImGui::Indent(20.0f);
	make_background_combo_box(tt.bg, "Background", current_theme);
	make_color_combo_box(tt.icon_color, "Icon color", current_theme);
	make_affine_transform_description(tt.icon_left, "Left edge", current_theme);
	make_affine_transform_description(tt.icon_top, "Top edge", current_theme);
	make_affine_transform_description(tt.icon_right, "Right edge", current_theme);
	make_affine_transform_description(tt.icon_bottom, "Bottom edge", current_theme);
	ImGui::Unindent(20.0f);
	ImGui::PopID();
}
void make_color_region_description(template_project::color_region& tt, char const* label, template_project::project& current_theme) {
	ImGui::PushID((void*)&tt);
	ImGui::Text(label);
	ImGui::Indent(20.0f);
	make_background_combo_box(tt.bg, "Background", current_theme);
	make_color_combo_box(tt.color, "Text color", current_theme);
	ImGui::Unindent(20.0f);
	ImGui::PopID();
}
void make_toggle_region_description(template_project::toggle_region& tt, char const* label, template_project::project& current_theme) {
	ImGui::PushID((void*)&tt);
	ImGui::Text(label);
	ImGui::Indent(20.0f);
	make_color_region_description(tt.primary, "Primary appearance", current_theme);
	make_color_region_description(tt.active, "Active appearance", current_theme);
	make_color_region_description(tt.disabled, "Disabled appearance", current_theme);
	make_font_combo_box(tt.font_choice, "Text font", current_theme);
	ImGui::InputFloat("Text scale", &tt.font_scale);
	{
		int32_t combo_selection = int32_t(tt.h_text_alignment);
		const char* new_template_options[] = { "left", "right", "center" };
		if(ImGui::Combo("H. text alignment", &combo_selection, new_template_options, 3)) {
			tt.h_text_alignment = template_project::aui_text_alignment(combo_selection);
		}
	}
	{
		int32_t combo_selection = int32_t(tt.v_text_alignment);
		const char* new_template_options[] = { "top", "bottom", "center" };
		if(ImGui::Combo("V. text alignment", &combo_selection, new_template_options, 3)) {
			tt.v_text_alignment = template_project::aui_text_alignment(combo_selection);
		}
	}
	make_affine_transform_description(tt.text_margin_left, "Text left margin", current_theme);
	make_affine_transform_description(tt.text_margin_right, "Text right margin", current_theme);
	make_affine_transform_description(tt.text_margin_top, "Text top margin", current_theme);
	make_affine_transform_description(tt.text_margin_bottom, "Text bottom margin", current_theme);

	ImGui::Unindent(20.0f);
	ImGui::PopID();
}
void make_mixed_region_description(template_project::mixed_region_template& tt, char const* label, template_project::project& current_theme) {
	ImGui::PushID((void*)&tt);
	ImGui::Text(label);
	ImGui::Indent(20.0f);
	make_background_combo_box(tt.bg, "Background", current_theme);
	make_color_combo_box(tt.shared_color, "Text and icon color", current_theme);

	make_font_combo_box(tt.font_choice, "Text font", current_theme);
	ImGui::InputFloat("Text scale", &tt.font_scale);
	ImGui::InputFloat("H. text margins (in grid units)", &tt.h_text_margins);
	ImGui::InputFloat("V. text margins (in grid units)", &tt.v_text_margins);
	{
		int32_t combo_selection = int32_t(tt.h_text_alignment);
		const char* new_template_options[] = { "left", "right", "center" };
		if(ImGui::Combo("H. text alignment", &combo_selection, new_template_options, 3)) {
			tt.h_text_alignment = template_project::aui_text_alignment(combo_selection);
		}
	}
	{
		int32_t combo_selection = int32_t(tt.v_text_alignment);
		const char* new_template_options[] = { "top", "bottom", "center" };
		if(ImGui::Combo("V. text alignment", &combo_selection, new_template_options, 3)) {
			tt.v_text_alignment = template_project::aui_text_alignment(combo_selection);
		}
	}

	make_affine_transform_description(tt.icon_left, "Icon left edge", current_theme);
	make_affine_transform_description(tt.icon_top, "Icon top edge", current_theme);
	make_affine_transform_description(tt.icon_right, "Icon right edge", current_theme);
	make_affine_transform_description(tt.icon_bottom, "Icon bottom edge", current_theme);
	ImGui::Unindent(20.0f);
	ImGui::PopID();
}

// Main code
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	glfwSetErrorCallback(glfw_error_callback);
	if(!glfwInit())
		return 1;

	// GL 3.0 + GLSL 130
	const char* glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	//glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
	//glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only

	// Create window with graphics context
	GLFWwindow* window = glfwCreateWindow(1280, 720, "Alice UI Editor", nullptr, nullptr);
	glfwSetScrollCallback(window, scroll_callback);
	glfwMaximizeWindow(window);

	if(window == nullptr)
		return 1;
	glfwMakeContextCurrent(window);
	assert(glewInit() == 0);
	glfwSwapInterval(1); // Enable vsync

	load_global_squares();
	load_shaders();

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsLight();

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	SetProcessDpiAwareness(PROCESS_DPI_AWARENESS::PROCESS_SYSTEM_DPI_AWARE);

	POINT ptZero = { 0, 0 };
	auto def_monitor = MonitorFromPoint(ptZero, MONITOR_DEFAULTTOPRIMARY);
	UINT xdpi = 0;
	UINT ydpi = 0;
	GetDpiForMonitor(def_monitor, MDT_EFFECTIVE_DPI, &xdpi, &ydpi);
	auto scale_value = xdpi != 0 ? float(xdpi) / 96.0f : 1.0f;
	auto text_scale = scale_value;
	HKEY reg_text_key = NULL;
	if(RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Accessibility", 0, KEY_NOTIFY | KEY_QUERY_VALUE, &reg_text_key) == ERROR_SUCCESS) {
		DWORD scale = 0;
		DWORD cb = sizeof scale;
		RegQueryValueExW(reg_text_key, L"TextScaleFactor", NULL, NULL, (LPBYTE)&scale, &cb);
		if(scale != 0) {
			text_scale *= float(scale) / 100.0f;
		}
		RegCloseKey(reg_text_key);
	}
	io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", text_scale * 18.0f);
	ImFontConfig config;
	config.MergeMode = true;
	io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segmdl2.ttf", text_scale * 12.0f, &config);
	ImGuiStyle& style = ImGui::GetStyle();
	ImGuiStyle styleold = style; // Backup colors
	style = ImGuiStyle(); // IMPORTANT: ScaleAllSizes will change the original size, so we should reset all style config
	style.WindowBorderSize = 1.0f;
	style.ChildBorderSize = 1.0f;
	style.PopupBorderSize = 1.0f;
	style.FrameBorderSize = 1.0f;
	style.TabBorderSize = 1.0f;
	style.WindowRounding = 0.0f;
	style.ChildRounding = 0.0f;
	style.PopupRounding = 0.0f;
	style.FrameRounding = 0.0f;
	style.ScrollbarRounding = 0.0f;
	style.GrabRounding = 0.0f;
	style.TabRounding = 0.0f;

	style.WindowRounding = 6.f;
	style.ChildRounding = 6.f;
	style.FrameRounding = 6.f;
	style.PopupRounding = 6.f;
	style.ScrollbarRounding = 6.f;
	style.GrabRounding = 2.f;
	style.TabRounding = 6.f;
	style.SeparatorTextBorderSize = 5.f;
	style.FrameBorderSize = 1.f;
	style.TabBorderSize = 1.f;


	//style.ScaleAllSizes(scale_value);
	CopyMemory(style.Colors, styleold.Colors, sizeof(style.Colors)); // Restore colors

	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	float ui_scale = std::max(1.0f, std::floor(text_scale + 0.5f));

	float drag_start_x = 0.0f;
	float drag_start_y = 0.0f;
	bool dragging = false;

	drag_target control_drag_target = drag_target::none;

	int32_t display_w = 0;
	int32_t display_h = 0;

	// Main loop
	while(!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		if(glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
			ImGui_ImplGlfw_Sleep(10);
			continue;
		}

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		//if(show_demo_window)
		// 
		//	ImGui::ShowDemoWindow();
		ImGui::SetNextWindowPos(ImVec2(10, 200), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);

		ImGui::Begin("Edit", NULL, ImGuiWindowFlags_None);

		if(ImGui::Button("New Project")) {
			auto new_file = fs::pick_new_file(L"tui");
			if(new_file.length() > 0) {
				auto breakpt = new_file.find_last_of(L'\\');
				auto rem = new_file.substr(breakpt + 1);
				auto ext_pos = rem.find_last_of(L'.');
				open_project.project_name = rem.substr(0, ext_pos);
				open_project.project_directory = new_file.substr(0, breakpt + 1);
			}
		}
		ImGui::SameLine();
		if(ImGui::Button("Open")) {
			auto new_file = fs::pick_existing_file(L"tui");
			if(new_file.length() > 0) {
				auto breakpt = new_file.find_last_of(L'\\');
				auto rem = new_file.substr(breakpt + 1);
				auto ext_pos = rem.find_last_of(L'.');
				fs::file loaded_file{ new_file };

				
				serialization::in_buffer file_content{ loaded_file.content().data, loaded_file.content().file_size };
				open_project = template_project::bytes_to_project(file_content);
				open_project.project_name = rem.substr(0, ext_pos);
				open_project.project_directory = new_file.substr(0, breakpt + 1);
				
				asvg::common_file_bank::bank.root_directory = open_project.project_directory + open_project.svg_directory;

				
					for(auto& i : open_project.icons) {
						fs::file loaded_file{ open_project.project_directory + open_project.svg_directory + fs::utf8_to_native(i.file_name) };
						i.renders = asvg::simple_svg(loaded_file.content().data, size_t(loaded_file.content().file_size));
					}
					for(auto& b : open_project.backgrounds) {
						fs::file loaded_file{ open_project.project_directory + open_project.svg_directory + fs::utf8_to_native(b.file_name) };
						b.renders = asvg::svg(loaded_file.content().data, size_t(loaded_file.content().file_size), b.base_x, b.base_y);
					}
				
			}
		}
		ImGui::SameLine();
		if(ImGui::Button("Save")) {
			serialization::out_buffer bytes;
			
			template_project::project_to_bytes(open_project, bytes);
			fs::write_file(open_project.project_directory + open_project.project_name + L".tui", bytes.data(), uint32_t(bytes.size()));
		}

		auto asssets_location = std::string("ASVG directory: ") + (open_project.svg_directory.empty() ? std::string("[none]") : fs::native_to_utf8(open_project.svg_directory));
		ImGui::Text(asssets_location.c_str());
		ImGui::SameLine();
		if(ImGui::Button("Change")) {
			auto new_dir = fs::pick_directory(open_project.project_directory) + L"\\";
			if(new_dir.length() > 1) {
				size_t common_length = 0;
				while(common_length < new_dir.size()) {
					auto next_common_length = new_dir.find_first_of(L'\\', common_length);
					if(new_dir.substr(0, next_common_length + 1) != open_project.project_directory.substr(0, next_common_length + 1)) {
						break;
					}
					common_length = next_common_length + size_t(1);
				}
				uint32_t missing_separators_count = 0;
				for(size_t i = common_length; i < open_project.project_directory.size(); ++i) {
					if(open_project.project_directory[i] == L'\\') {
						++missing_separators_count;
					}
				}
				if(missing_separators_count == 0) {
					if(common_length >= new_dir.size()) {
						open_project.svg_directory = L"";
					} else {
						open_project.svg_directory = new_dir.substr(common_length);
					}
				} else {
					std::wstring temp;
					for(uint32_t i = 0; i < missing_separators_count; ++i) {
						temp += L"..\\";
					}
					if(common_length >= new_dir.size()) {
						open_project.svg_directory = temp;
					} else {
						open_project.svg_directory = temp + new_dir.substr(common_length);
					}
				}

				asvg::common_file_bank::bank.root_directory = open_project.project_directory + open_project.svg_directory;
			}
		}
		ImGui::Text("-----------------");
		auto& thm = open_project;

		if(ImGui::TreeNodeEx("Colors", base_tree_flags)) {
			for(auto& c : thm.colors) {
				if(ImGui::TreeNodeEx(c.display_name.c_str(), base_tree_flags)) {
					make_name_change(c.temp_display_name, c.display_name, thm.colors);
					{
						float ccolor[4] = { c.r, c.g, c.b, c.a };
						ImGui::ColorEdit4("Color", ccolor);
						c.r = ccolor[0];
						c.g = ccolor[1];
						c.b = ccolor[2];
						c.a = ccolor[3];
					}
					ImGui::TreePop();
				}
			}

			if(ImGui::Button("Add color")) {
				thm.colors.emplace_back();
				thm.colors.back().display_name = "new color";
			}
			if(!thm.colors.empty()) {
				if(ImGui::Button("Delete color")) {
					thm.colors.pop_back();
				}
			}
			ImGui::TreePop();
		}
		if(ImGui::TreeNodeEx("Backgrounds", base_tree_flags)) {
			for(auto& b : thm.backgrounds) {
				auto flags = base_tree_flags | (selected_type == template_project::template_type::background && selected_template == int32_t(std::distance(thm.backgrounds.data(), &b)) ? ImGuiTreeNodeFlags_Selected : 0);
				if(ImGui::TreeNodeEx(b.file_name.empty() ? "No File" : b.file_name.c_str(), flags)) {
					if(ImGui::IsItemClicked()) {
						selected_type = template_project::template_type::background;
						selected_template = int32_t(std::distance(thm.backgrounds.data(), &b));
					}

					if(ImGui::Button("Pick file")) {
						auto new_file = fs::pick_existing_file_from_folder(L"asvg", open_project.project_directory + open_project.svg_directory);
						if(new_file.length() > 0) {
							auto breakpt = new_file.find_last_of(L'\\');
							std::wstring rem;
							if(breakpt != std::wstring::npos) {
								rem = new_file.substr((open_project.project_directory + open_project.svg_directory).length());
							} else {
								rem = new_file;
							}

							b.file_name = fs::native_to_utf8(rem);
							fs::file loaded_file{ new_file };
							b.renders = asvg::svg(loaded_file.content().data, size_t(loaded_file.content().file_size), b.base_x, b.base_y);
						}
					}
					if(!b.file_name.empty()) {
						if(ImGui::Button("Reload")) {
							fs::file loaded_file{ open_project.project_directory + open_project.svg_directory + fs::utf8_to_native(b.file_name) };
							b.renders = asvg::svg(loaded_file.content().data, size_t(loaded_file.content().file_size), b.base_x, b.base_y);
						}
					}


					if(ImGui::InputInt("Base width", &b.base_x)) {
						b.renders.base_width = b.base_x;
						b.renders.release_renders();
					}
					if(ImGui::InputInt("Base height", &b.base_y)) {
						b.renders.base_height = b.base_y;
						b.renders.release_renders();
					}

					ImGui::TreePop();
				}
			}

			if(ImGui::Button("Add background")) {
				thm.backgrounds.emplace_back();
			}
			if(!thm.backgrounds.empty()) {
				if(ImGui::Button("Delete background")) {
					thm.backgrounds.pop_back();
				}
			}
			ImGui::TreePop();
		}
		if(ImGui::TreeNodeEx("Icons", base_tree_flags)) {
			for(auto& b : thm.icons) {
				auto flags = base_tree_flags | (selected_type == template_project::template_type::icon && selected_template == int32_t(std::distance(thm.icons.data(), &b)) ? ImGuiTreeNodeFlags_Selected : 0);
				if(ImGui::TreeNodeEx(b.file_name.empty() ? "No File" : b.file_name.c_str(), flags)) {
					if(ImGui::IsItemClicked()) {
						selected_type = template_project::template_type::icon;
						selected_template = int32_t(std::distance(thm.icons.data(), &b));

					}


					if(ImGui::Button("Pick file")) {
						auto new_file = fs::pick_existing_file_from_folder(L"svg", open_project.project_directory + open_project.svg_directory);
						if(new_file.length() > 0) {
							auto breakpt = new_file.find_last_of(L'\\');
							std::wstring rem;
							if(breakpt != std::wstring::npos) {
								rem = new_file.substr((open_project.project_directory + open_project.svg_directory).length() );
							} else {
								rem = new_file;
							}

							b.file_name = fs::native_to_utf8(rem);
							fs::file loaded_file{ new_file };
							b.renders = asvg::simple_svg(loaded_file.content().data, size_t(loaded_file.content().file_size));
						}
					}
					if(!b.file_name.empty()) {
						if(ImGui::Button("Reload")) {
							fs::file loaded_file{ open_project.project_directory + open_project.svg_directory + fs::utf8_to_native(b.file_name) };
							b.renders = asvg::simple_svg(loaded_file.content().data, size_t(loaded_file.content().file_size));
						}
					}

					ImGui::TreePop();
				}
			}

			if(ImGui::Button("Add icon")) {
				thm.icons.emplace_back();
			}
			if(!thm.icons.empty()) {
				if(ImGui::Button("Delete icon")) {
					thm.icons.pop_back();
				}
			}
			ImGui::TreePop();
		}
		if(ImGui::TreeNodeEx("Label templates", base_tree_flags)) {
			for(auto& i : thm.label_t) {
				auto flags = base_tree_flags | (selected_type == template_project::template_type::label && selected_template == int32_t(std::distance(thm.label_t.data(), &i)) ? ImGuiTreeNodeFlags_Selected : 0);
				if(ImGui::TreeNodeEx(i.display_name.c_str(), flags)) {
					if(ImGui::IsItemClicked()) {
						selected_type = template_project::template_type::label;
						selected_template = int32_t(std::distance(thm.label_t.data(), &i));
					}

					make_name_change(i.temp_display_name, i.display_name, thm.label_t);
					make_text_region_description(i.primary, "Definition", thm);

					ImGui::TreePop();
				}
			}
			if(ImGui::Button("Add label")) {
				thm.label_t.emplace_back();
				thm.label_t.back().display_name = "new label";
			}
			if(!thm.label_t.empty()) {
				if(ImGui::Button("Delete label")) {
					thm.label_t.pop_back();
				}
			}
			ImGui::TreePop();
		}
		if(ImGui::TreeNodeEx("Button templates", base_tree_flags)) {
			for(auto& i : thm.button_t) {
				auto flags = base_tree_flags | (selected_type == template_project::template_type::button && selected_template == int32_t(std::distance(thm.button_t.data(), &i)) ? ImGuiTreeNodeFlags_Selected : 0);
				if(ImGui::TreeNodeEx(i.display_name.c_str(), flags)) {
					if(ImGui::IsItemClicked()) {
						selected_type = template_project::template_type::button;
						selected_template = int32_t(std::distance(thm.button_t.data(), &i));
					}

					make_name_change(i.temp_display_name, i.display_name, thm.button_t);

					make_text_region_description(i.primary, "Standard appearance", thm);
					make_text_region_description(i.active, "Active appearance", thm);
					make_text_region_description(i.disabled, "Disabled appearance", thm);
					ImGui::Checkbox("Animate active transition", &i.animate_active_transition);

					ImGui::TreePop();
				}
			}
			if(ImGui::Button("Add button")) {
				thm.button_t.emplace_back();
				thm.button_t.back().display_name = "new button";
			}
			if(!thm.button_t.empty()) {
				if(ImGui::Button("Delete button")) {
					thm.button_t.pop_back();
				}
			}
			ImGui::TreePop();
		}
		if(ImGui::TreeNodeEx("Icon button templates", base_tree_flags)) {
			for(auto& i : thm.iconic_button_t) {
				auto flags = base_tree_flags | (selected_type == template_project::template_type::iconic_button && selected_template == int32_t(std::distance(thm.iconic_button_t.data(), &i)) ? ImGuiTreeNodeFlags_Selected : 0);
				if(ImGui::TreeNodeEx(i.display_name.c_str(), flags)) {
					if(ImGui::IsItemClicked()) {
						selected_type = template_project::template_type::iconic_button;
						selected_template = int32_t(std::distance(thm.iconic_button_t.data(), &i));
					}

					make_name_change(i.temp_display_name, i.display_name, thm.iconic_button_t);

					make_icon_region_description(i.primary, "Standard appearance", thm);
					make_icon_region_description(i.active, "Active appearance", thm);
					make_icon_region_description(i.disabled, "Disabled appearance", thm);
					ImGui::Checkbox("Animate active transition", &i.animate_active_transition);

					ImGui::TreePop();
				}
			}
			if(ImGui::Button("Add icon button")) {
				thm.iconic_button_t.emplace_back();
				thm.iconic_button_t.back().display_name = "new icon button";
			}
			if(!thm.iconic_button_t.empty()) {
				if(ImGui::Button("Delete icon button")) {
					thm.iconic_button_t.pop_back();
				}
			}
			ImGui::TreePop();
		}
		if(ImGui::TreeNodeEx("Text & icon button templates", base_tree_flags)) {
			for(auto& i : thm.mixed_button_t) {
				auto flags = base_tree_flags | (selected_type == template_project::template_type::mixed_button && selected_template == int32_t(std::distance(thm.mixed_button_t.data(), &i)) ? ImGuiTreeNodeFlags_Selected : 0);
				if(ImGui::TreeNodeEx(i.display_name.c_str(), flags)) {
					if(ImGui::IsItemClicked()) {
						selected_type = template_project::template_type::mixed_button;
						selected_template = int32_t(std::distance(thm.mixed_button_t.data(), &i));
					}

					make_name_change(i.temp_display_name, i.display_name, thm.mixed_button_t);

					make_mixed_region_description(i.primary, "Standard appearance", thm);
					make_mixed_region_description(i.active, "Active appearance", thm);
					make_mixed_region_description(i.disabled, "Disabled appearance", thm);
					ImGui::Checkbox("Animate active transition", &i.animate_active_transition);

					ImGui::TreePop();
				}
			}
			if(ImGui::Button("Add text & icon button")) {
				thm.mixed_button_t.emplace_back();
				thm.mixed_button_t.back().display_name = "new text and icon button";
			}
			if(!thm.mixed_button_t.empty()) {
				if(ImGui::Button("Delete text & icon button")) {
					thm.mixed_button_t.pop_back();
				}
			}
			ImGui::TreePop();
		}
		if(ImGui::TreeNodeEx("Toggle button templates", base_tree_flags)) {
			for(auto& i : thm.toggle_button_t) {
				auto flags = base_tree_flags | (selected_type == template_project::template_type::toggle_button && selected_template == int32_t(std::distance(thm.toggle_button_t.data(), &i)) ? ImGuiTreeNodeFlags_Selected : 0);
				if(ImGui::TreeNodeEx(i.display_name.c_str(), flags)) {
					if(ImGui::IsItemClicked()) {
						selected_type = template_project::template_type::toggle_button;
						selected_template = int32_t(std::distance(thm.toggle_button_t.data(), &i));
					}

					make_name_change(i.temp_display_name, i.display_name, thm.toggle_button_t);

					make_toggle_region_description(i.on_region, "On apperance", thm);
					make_toggle_region_description(i.off_region, "Off apperance", thm);

					ImGui::Checkbox("Animate active transition", &i.animate_active_transition);

					ImGui::TreePop();
				}
			}
			if(ImGui::Button("Add toggle button")) {
				thm.toggle_button_t.emplace_back();
				thm.toggle_button_t.back().display_name = "new toggle button";
			}
			if(!thm.toggle_button_t.empty()) {
				if(ImGui::Button("Delete toggle button")) {
					thm.toggle_button_t.pop_back();
				}
			}
			ImGui::TreePop();
		}
		if(ImGui::TreeNodeEx("Progress bar templates", base_tree_flags)) {
			for(auto& i : thm.progress_bar_t) {
				auto flags = base_tree_flags | (selected_type == template_project::template_type::progress_bar && selected_template == int32_t(std::distance(thm.progress_bar_t.data(), &i)) ? ImGuiTreeNodeFlags_Selected : 0);
				if(ImGui::TreeNodeEx(i.display_name.c_str(), flags)) {
					if(ImGui::IsItemClicked()) {
						selected_type = template_project::template_type::progress_bar;
						selected_template = int32_t(std::distance(thm.progress_bar_t.data(), &i));
					}

					make_name_change(i.temp_display_name, i.display_name, thm.progress_bar_t);

					make_background_combo_box(i.bg_a, "'off' background", thm);
					make_background_combo_box(i.bg_b, "'on' background", thm);
					make_color_combo_box(i.text_color, "Text color", thm);
					make_font_combo_box(i.font_choice, "Font", thm);
					ImGui::InputFloat("H. margins (in grid units)", &i.h_text_margins);
					ImGui::InputFloat("V. margins (in grid units)", &i.v_text_margins);
					{
						int32_t combo_selection = int32_t(i.h_text_alignment);
						const char* new_template_options[] = { "left", "right", "center" };
						if(ImGui::Combo("H. alignment", &combo_selection, new_template_options, 3)) {
							i.h_text_alignment = template_project::aui_text_alignment(combo_selection);
						}
					}
					{
						int32_t combo_selection = int32_t(i.v_text_alignment);
						const char* new_template_options[] = { "top", "bottom", "center" };
						if(ImGui::Combo("V. alignment", &combo_selection, new_template_options, 3)) {
							i.v_text_alignment = template_project::aui_text_alignment(combo_selection);
						}
					}
					ImGui::Checkbox("Display percentage text", &i.display_percentage_text);

					ImGui::TreePop();
				}
			}
			if(ImGui::Button("Add progress bar")) {
				thm.progress_bar_t.emplace_back();
				thm.progress_bar_t.back().display_name = "new progress bar";
			}
			if(!thm.progress_bar_t.empty()) {
				if(ImGui::Button("Delete progress bar")) {
					thm.progress_bar_t.pop_back();
				}
			}
			ImGui::TreePop();
		}
		if(ImGui::TreeNodeEx("Window templates", base_tree_flags)) {
			for(auto& i : thm.window_t) {
				auto flags = base_tree_flags | (selected_type == template_project::template_type::window && selected_template == int32_t(std::distance(thm.window_t.data(), &i)) ? ImGuiTreeNodeFlags_Selected : 0);
				if(ImGui::TreeNodeEx(i.display_name.c_str(), flags)) {
					if(ImGui::IsItemClicked()) {
						selected_type = template_project::template_type::window;
						selected_template = int32_t(std::distance(thm.window_t.data(), &i));
					}

					make_name_change(i.temp_display_name, i.display_name, thm.window_t);

					make_background_combo_box(i.bg, "Background", thm);
					make_layout_region_combo_box(i.layout_region_definition, "Default layout region", thm);
					make_icon_button_combo_box(i.close_button_definition, "Close button appearance", thm);
					make_icon_combo_box(i.close_button_icon, "Close button icon", thm);
					ImGui::InputFloat("H. close button margin (in grid units)", &i.h_close_button_margin);
					ImGui::InputFloat("V. close button margin (in grid units)", &i.v_close_button_margin);

					ImGui::TreePop();
				}
			}
			if(ImGui::Button("Add window")) {
				thm.window_t.emplace_back();
				thm.window_t.back().display_name = "new window";
			}
			if(!thm.window_t.empty()) {
				if(ImGui::Button("Delete window")) {
					thm.window_t.pop_back();
				}
			}
			ImGui::TreePop();
		}
		if(ImGui::TreeNodeEx("Layout region templates", base_tree_flags)) {
			for(auto& i : thm.layout_region_t) {
				auto flags = base_tree_flags | (selected_type == template_project::template_type::layout_region && selected_template == int32_t(std::distance(thm.layout_region_t.data(), &i)) ? ImGuiTreeNodeFlags_Selected : 0);
				if(ImGui::TreeNodeEx(i.display_name.c_str(), flags)) {
					if(ImGui::IsItemClicked()) {
						selected_type = template_project::template_type::layout_region;
						selected_template = int32_t(std::distance(thm.layout_region_t.data(), &i));
					}

					make_name_change(i.temp_display_name, i.display_name, thm.layout_region_t);

					make_background_combo_box(i.bg, "'Background", thm);
					make_icon_button_combo_box(i.left_button, "Left button appearance", thm);
					make_icon_combo_box(i.left_button_icon, "Left button icon", thm);
					make_icon_button_combo_box(i.right_button, "Right button appearance", thm);
					make_icon_combo_box(i.right_button_icon, "Right button icon", thm);
					make_text_region_description(i.page_number_text, "Page numbers", thm);

					ImGui::TreePop();
				}
			}
			if(ImGui::Button("Add layout region")) {
				thm.layout_region_t.emplace_back();
				thm.layout_region_t.back().display_name = "new layout region";
			}
			if(!thm.layout_region_t.empty()) {
				if(ImGui::Button("Delete layout region")) {
					thm.layout_region_t.pop_back();
				}
			}
			ImGui::TreePop();
		}




		ImGui::End();


		if(last_scroll_value > 0.0f) {
			if(!io.WantCaptureMouse)
				ui_scale += 1.0f;
			last_scroll_value = 0.0f;
		} else if(last_scroll_value < 0.0f) {
			if(!io.WantCaptureMouse && ui_scale > 1.0f)
				ui_scale -= 1.0f;
			last_scroll_value = 0.0f;
		}

		if(io.MouseDown[0]) {
			if(!dragging && !io.WantCaptureMouse) {
				drag_start_x = io.MousePos.x - drag_offset_x;
				drag_start_y = io.MousePos.y - drag_offset_y;
				dragging = true;
			}
			if(dragging) {
				drag_offset_x = io.MousePos.x - drag_start_x;
				drag_offset_y = io.MousePos.y - drag_start_y;
				//ImGui::SetMouseCursor(ImGuiMouseCursor_::ImGuiMouseCursor_ResizeAll);
			} 
		} else {
			if(dragging) {
				dragging = false;
			} else if(!io.WantCaptureMouse) {
				//ImGui::SetTooltip("%s", win.wrapped.name.c_str());
			}
		}

		// Rendering
		ImGui::Render();

		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		glUseProgram(ui_shader_program);
		glUniform1i(glGetUniformLocation(ui_shader_program, "texture_sampler"), 0);
		glUniform1f(glGetUniformLocation(ui_shader_program, "screen_width"), float(display_w));
		glUniform1f(glGetUniformLocation(ui_shader_program, "screen_height"), float(display_h));
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		

		//if(0 <= selected_window && selected_window < int32_t(open_project.windows.size())) {
		//	auto& win = open_project.windows[selected_window];
		//	bool highlightwin = selected_control == -1 && (test_rect_target(io.MousePos.x, io.MousePos.y, win.wrapped.x_pos * ui_scale + drag_offset_x, win.wrapped.y_pos * ui_scale + drag_offset_y, win.wrapped.x_size * ui_scale, win.wrapped.y_size * ui_scale, ui_scale) != drag_target::none || control_drag_target != drag_target::none);
		//	render_window(win, (win.wrapped.x_pos + drag_offset_x / ui_scale), (win.wrapped.y_pos + drag_offset_y / ui_scale), highlightwin, ui_scale);
		//}

		//
		// Draw Grid
		//
		{
			glBindVertexArray(global_square_vao);
			glBindVertexBuffer(0, global_square_buffer, 0, sizeof(GLfloat) * 4);
			glUniform4f(glGetUniformLocation(ui_shader_program, "d_rect"), 0.0f, 0.0f, float(display_w), float(display_h));
			//glUniform1f(glGetUniformLocation(ui_shader_program, "grid_size"), ui_scale * float(open_project.grid_size));
			glUniform1f(glGetUniformLocation(ui_shader_program, "grid_size"), ui_scale * 8.0f);
			glUniform2f(glGetUniformLocation(ui_shader_program, "grid_off"), std::floor(-drag_offset_x), std::floor(-drag_offset_y));
			GLuint subroutines[2] = { 4, 0 };
			glUniform2ui(glGetUniformLocation(ui_shader_program, "subroutines_index"), subroutines[0], subroutines[1]);
		}
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

			auto render_asvg_rect = [&](asvg::svg& s, int32_t& hcursor, int32_t vcursor, int32_t& line_vcursor, int32_t x_sz, int32_t y_sz, int32_t gsz) {
				render_hollow_rect(color3f{ 1.f, 0.f, 0.f },
					drag_offset_x + hcursor,
					drag_offset_y + vcursor,
					std::max(1, int32_t(gsz * x_sz * ui_scale)),
					std::max(1, int32_t(gsz* y_sz * ui_scale)));
				
				render_textured_rect(color3f{ 0.f, 0.f, 0.f },
					drag_offset_x + hcursor,
					drag_offset_y + vcursor,
					std::max(1, int32_t(gsz* x_sz * ui_scale)),
					std::max(1, int32_t(gsz* y_sz * ui_scale)),
					s.get_render( x_sz, y_sz, gsz, 2.0f));

				hcursor += int32_t(gsz * x_sz * ui_scale) + int32_t(8 * ui_scale);
				line_vcursor = std::max(line_vcursor, vcursor + int32_t(gsz * y_sz * ui_scale) + int32_t(8 * ui_scale));
			};
			auto render_svg_rect = [&](asvg::simple_svg& s, int32_t& hcursor, int32_t vcursor, int32_t& line_vcursor, int32_t x_sz, int32_t y_sz, color3f c) {
				
				render_hollow_rect(color3f{ 1.f, 0.f, 0.f },
					drag_offset_x + hcursor,
					drag_offset_y + vcursor,
					std::max(1, int32_t(x_sz * ui_scale)),
					std::max(1, int32_t(y_sz * ui_scale)));
					

				render_textured_rect(color3f{ 0.f, 0.f, 0.f },
					drag_offset_x + hcursor,
					drag_offset_y + vcursor,
					std::max(1, int32_t(x_sz * ui_scale)),
					std::max(1, int32_t(y_sz * ui_scale)),
					s.get_render(x_sz, y_sz, 2.0f, c.r, c.g, c.b));

				hcursor += int32_t(x_sz * ui_scale) + int32_t(8 * ui_scale);
				line_vcursor = std::max(line_vcursor, vcursor + int32_t(y_sz * ui_scale) + int32_t(8 * ui_scale));
			};

			switch(selected_type) {
				case template_project::template_type::background:
					if(0 <= selected_template && selected_template < int32_t(thm.backgrounds.size()) && !thm.backgrounds[selected_template].file_name.empty()) {
						int32_t hcursor = 0;
						int32_t vcursor = 0;
						int32_t next_line = 0;
						render_asvg_rect(thm.backgrounds[selected_template].renders, hcursor, vcursor, next_line, 4, 4, 8);
						render_asvg_rect(thm.backgrounds[selected_template].renders, hcursor, vcursor, next_line, 4, 4, 9);
						render_asvg_rect(thm.backgrounds[selected_template].renders, hcursor, vcursor, next_line, 4, 4, 10);
						//render_asvg_rect(thm.backgrounds[selected_template].renders, hcursor, vcursor, next_line, 6, 2, 47);
						//render_asvg_rect(thm.backgrounds[selected_template].renders, hcursor, vcursor, next_line, 6, 2, 57);
						vcursor = next_line;
						hcursor = 0;
						render_asvg_rect(thm.backgrounds[selected_template].renders, hcursor, vcursor, next_line, 8, 4, 8);
						render_asvg_rect(thm.backgrounds[selected_template].renders, hcursor, vcursor, next_line, 4, 8, 8);
						
					}
					break;
				case template_project::template_type::button:
					if(0 <= selected_template && selected_template < int32_t(thm.button_t.size())) {
						int32_t hcursor = 0;
						int32_t vcursor = 0;
						int32_t next_line = 0;
						if(thm.button_t[selected_template].primary.bg != -1) {
							render_asvg_rect(thm.backgrounds[thm.button_t[selected_template].primary.bg].renders, hcursor, vcursor, next_line, 12, 3, 8);
						}
						vcursor = next_line;
						hcursor = 0;
						if(thm.button_t[selected_template].active.bg != -1) {
							render_asvg_rect(thm.backgrounds[thm.button_t[selected_template].active.bg].renders, hcursor, vcursor, next_line, 12, 3, 8);
						}
						vcursor = next_line;
						hcursor = 0;
						if(thm.button_t[selected_template].disabled.bg != -1) {
							render_asvg_rect(thm.backgrounds[thm.button_t[selected_template].disabled.bg].renders, hcursor, vcursor, next_line, 12, 3, 8);
						}
					}
					break;
				case template_project::template_type::color:
					break;
				case template_project::template_type::icon:
					if(0 <= selected_template && selected_template < int32_t(thm.icons.size()) && !thm.icons[selected_template].file_name.empty()) {
						int32_t hcursor = 0;
						int32_t vcursor = 0;
						int32_t next_line = 0;
						render_svg_rect(thm.icons[selected_template].renders, hcursor, vcursor, next_line, 16, 16, color3f{ 0.0f, 0.0f, 0.0f });
						render_svg_rect(thm.icons[selected_template].renders, hcursor, vcursor, next_line, 40, 40, color3f{ 0.0f, 0.0f, 0.0f });
						render_svg_rect(thm.icons[selected_template].renders, hcursor, vcursor, next_line, 40, 40, color3f{ 1.0f, 0.1f, 0.1f });
					}
					break;
				case template_project::template_type::iconic_button:
					if(0 <= selected_template && selected_template < int32_t(thm.iconic_button_t.size())) {
						int32_t hcursor = 0;
						int32_t vcursor = 0;
						int32_t next_line = 0;
						if(thm.iconic_button_t[selected_template].primary.bg != -1) {
							auto l = thm.iconic_button_t[selected_template].primary.icon_left.resolve(float(4 * 8), float(4 * 8), 8.0f) + hcursor ;
							auto t = thm.iconic_button_t[selected_template].primary.icon_top.resolve(float(4 * 8), float(4 * 8), 8.0f) + vcursor;
							auto r = thm.iconic_button_t[selected_template].primary.icon_right.resolve(float(4 * 8), float(4 * 8), 8.0f) + hcursor;
							auto b = thm.iconic_button_t[selected_template].primary.icon_bottom.resolve(float(4 * 8), float(4 * 8), 8.0f) + vcursor;
							render_asvg_rect(thm.backgrounds[thm.iconic_button_t[selected_template].primary.bg].renders, hcursor, vcursor, next_line, 4, 4, 8);
							if(thm.iconic_button_t[selected_template].primary.icon_color < int32_t(thm.colors.size())) {
								render_hollow_rect(
									color3f{
										thm.colors[thm.iconic_button_t[selected_template].primary.icon_color].r,
										thm.colors[thm.iconic_button_t[selected_template].primary.icon_color].g,
										thm.colors[thm.iconic_button_t[selected_template].primary.icon_color].b },
									drag_offset_x + l * ui_scale,
									drag_offset_y + t * ui_scale,
									std::max(1, int32_t((r - l) * ui_scale)),
									std::max(1, int32_t((b - t) * ui_scale)));
							} else {
								render_hollow_rect(color3f{ 1.f, 0.f, 0.f },
									drag_offset_x + l * ui_scale,
									drag_offset_y + t * ui_scale,
									std::max(1, int32_t((r - l) * ui_scale)),
									std::max(1, int32_t((b - t) * ui_scale)));
							}
						}
						vcursor = next_line;
						hcursor = 0;
						if(thm.iconic_button_t[selected_template].active.bg != -1) {
							auto l = thm.iconic_button_t[selected_template].active.icon_left.resolve(float(4 * 8), float(4 * 8), 8.0f) * ui_scale + hcursor;
							auto t = thm.iconic_button_t[selected_template].active.icon_top.resolve(float(4 * 8), float(4 * 8), 8.0f) * ui_scale + vcursor;
							auto r = thm.iconic_button_t[selected_template].active.icon_right.resolve(float(4 * 8), float(4 * 8), 8.0f) * ui_scale + hcursor;
							auto b = thm.iconic_button_t[selected_template].active.icon_bottom.resolve(float(4 * 8), float(4 * 8), 8.0f) * ui_scale + vcursor;
							render_asvg_rect(thm.backgrounds[thm.iconic_button_t[selected_template].active.bg].renders, hcursor, vcursor, next_line, 4, 4, 8);
							if(thm.iconic_button_t[selected_template].active.icon_color < int32_t(thm.colors.size())) {
								render_hollow_rect(
									color3f{
										thm.colors[thm.iconic_button_t[selected_template].active.icon_color].r,
										thm.colors[thm.iconic_button_t[selected_template].active.icon_color].g,
										thm.colors[thm.iconic_button_t[selected_template].active.icon_color].b },
										drag_offset_x + l,
										drag_offset_y + t,
										std::max(1, int32_t((r - l))),
										std::max(1, int32_t((b - t))));
							} else {
								render_hollow_rect(color3f{ 1.f, 0.f, 0.f },
									drag_offset_x + l,
									drag_offset_y + t,
									std::max(1, int32_t((r - l))),
									std::max(1, int32_t((b - t))));
							}
						}
						vcursor = next_line;
						hcursor = 0;
						if(thm.iconic_button_t[selected_template].disabled.bg != -1) {
							auto l = thm.iconic_button_t[selected_template].disabled.icon_left.resolve(float(4 * 8), float(4 * 8), 8.0f) * ui_scale + hcursor;
							auto t = thm.iconic_button_t[selected_template].disabled.icon_top.resolve(float(4 * 8), float(4 * 8), 8.0f) * ui_scale + vcursor;
							auto r = thm.iconic_button_t[selected_template].disabled.icon_right.resolve(float(4 * 8), float(4 * 8), 8.0f) * ui_scale + hcursor;
							auto b = thm.iconic_button_t[selected_template].disabled.icon_bottom.resolve(float(4 * 8), float(4 * 8), 8.0f) * ui_scale + vcursor;
							render_asvg_rect(thm.backgrounds[thm.iconic_button_t[selected_template].disabled.bg].renders, hcursor, vcursor, next_line, 4, 4, 8);
							if(thm.iconic_button_t[selected_template].disabled.icon_color < int32_t(thm.colors.size())) {
								render_hollow_rect(
									color3f{
										thm.colors[thm.iconic_button_t[selected_template].disabled.icon_color].r,
										thm.colors[thm.iconic_button_t[selected_template].disabled.icon_color].g,
										thm.colors[thm.iconic_button_t[selected_template].disabled.icon_color].b },
										drag_offset_x + l ,
										drag_offset_y + t ,
										std::max(1, int32_t((r - l) )),
										std::max(1, int32_t((b - t) )));
							} else {
								render_hollow_rect(color3f{ 1.f, 0.f, 0.f },
									drag_offset_x + l ,
									drag_offset_y + t ,
									std::max(1, int32_t((r - l) )),
									std::max(1, int32_t((b - t) )));
							}
						}
					}
					break;
				case template_project::template_type::mixed_button:
					if(0 <= selected_template && selected_template < int32_t(thm.mixed_button_t.size())) {
						int32_t hcursor = 0;
						int32_t vcursor = 0;
						int32_t next_line = 0;
						if(thm.mixed_button_t[selected_template].primary.bg != -1) {
							auto l = thm.mixed_button_t[selected_template].primary.icon_left.resolve(float(16 * 8), float(2 * 8), 8.0f) + hcursor;
							auto t = thm.mixed_button_t[selected_template].primary.icon_top.resolve(float(16 * 8), float(2 * 8), 8.0f) + vcursor;
							auto r = thm.mixed_button_t[selected_template].primary.icon_right.resolve(float(16 * 8), float(2 * 8), 8.0f) + hcursor;
							auto b = thm.mixed_button_t[selected_template].primary.icon_bottom.resolve(float(16 * 8), float(2 * 8), 8.0f) + vcursor;
							render_asvg_rect(thm.backgrounds[thm.mixed_button_t[selected_template].primary.bg].renders, hcursor, vcursor, next_line, 16, 2, 8);
							if(thm.mixed_button_t[selected_template].primary.shared_color < int32_t(thm.colors.size())) {
								render_hollow_rect(
									color3f{
										thm.colors[thm.mixed_button_t[selected_template].primary.shared_color].r,
										thm.colors[thm.mixed_button_t[selected_template].primary.shared_color].g,
										thm.colors[thm.mixed_button_t[selected_template].primary.shared_color].b },
										drag_offset_x + l * ui_scale,
										drag_offset_y + t * ui_scale,
										std::max(1, int32_t((r - l) * ui_scale)),
										std::max(1, int32_t((b - t) * ui_scale)));
							} else {
								render_hollow_rect(color3f{ 1.f, 0.f, 0.f },
									drag_offset_x + l * ui_scale,
									drag_offset_y + t * ui_scale,
									std::max(1, int32_t((r - l) * ui_scale)),
									std::max(1, int32_t((b - t) * ui_scale)));
							}
						}
						vcursor = next_line;
						hcursor = 0;
						if(thm.mixed_button_t[selected_template].active.bg != -1) {
							auto l = thm.mixed_button_t[selected_template].active.icon_left.resolve(float(16 * 8), float(2 * 8), 8.0f) * ui_scale + hcursor;
							auto t = thm.mixed_button_t[selected_template].active.icon_top.resolve(float(16 * 8), float(2 * 8), 8.0f) * ui_scale + vcursor;
							auto r = thm.mixed_button_t[selected_template].active.icon_right.resolve(float(16 * 8), float(2 * 8), 8.0f) * ui_scale + hcursor;
							auto b = thm.mixed_button_t[selected_template].active.icon_bottom.resolve(float(16 * 8), float(2 * 8), 8.0f) * ui_scale + vcursor;
							render_asvg_rect(thm.backgrounds[thm.mixed_button_t[selected_template].active.bg].renders, hcursor, vcursor, next_line, 16, 2, 8);
							if(thm.mixed_button_t[selected_template].active.shared_color < int32_t(thm.colors.size())) {
								render_hollow_rect(
									color3f{
										thm.colors[thm.mixed_button_t[selected_template].active.shared_color].r,
										thm.colors[thm.mixed_button_t[selected_template].active.shared_color].g,
										thm.colors[thm.mixed_button_t[selected_template].active.shared_color].b },
										drag_offset_x + l,
										drag_offset_y + t,
										std::max(1, int32_t((r - l))),
										std::max(1, int32_t((b - t))));
							} else {
								render_hollow_rect(color3f{ 1.f, 0.f, 0.f },
									drag_offset_x + l,
									drag_offset_y + t,
									std::max(1, int32_t((r - l))),
									std::max(1, int32_t((b - t))));
							}
						}
						vcursor = next_line;
						hcursor = 0;
						if(thm.mixed_button_t[selected_template].disabled.bg != -1) {
							auto l = thm.mixed_button_t[selected_template].disabled.icon_left.resolve(float(16 * 8), float(2 * 8), 8.0f) * ui_scale + hcursor;
							auto t = thm.mixed_button_t[selected_template].disabled.icon_top.resolve(float(16 * 8), float(2 * 8), 8.0f) * ui_scale + vcursor;
							auto r = thm.mixed_button_t[selected_template].disabled.icon_right.resolve(float(16 * 8), float(2 * 8), 8.0f) * ui_scale + hcursor;
							auto b = thm.mixed_button_t[selected_template].disabled.icon_bottom.resolve(float(16 * 8), float(2 * 8), 8.0f) * ui_scale + vcursor;
							render_asvg_rect(thm.backgrounds[thm.mixed_button_t[selected_template].disabled.bg].renders, hcursor, vcursor, next_line, 16, 2, 8);
							if(thm.mixed_button_t[selected_template].disabled.shared_color < int32_t(thm.colors.size())) {
								render_hollow_rect(
									color3f{
										thm.colors[thm.mixed_button_t[selected_template].disabled.shared_color].r,
										thm.colors[thm.mixed_button_t[selected_template].disabled.shared_color].g,
										thm.colors[thm.mixed_button_t[selected_template].disabled.shared_color].b },
										drag_offset_x + l,
										drag_offset_y + t,
										std::max(1, int32_t((r - l))),
										std::max(1, int32_t((b - t))));
							} else {
								render_hollow_rect(color3f{ 1.f, 0.f, 0.f },
									drag_offset_x + l,
									drag_offset_y + t,
									std::max(1, int32_t((r - l))),
									std::max(1, int32_t((b - t))));
							}
						}
					}
					break;
				case template_project::template_type::toggle_button:
					if(0 <= selected_template && selected_template < int32_t(thm.toggle_button_t.size())) {
						int32_t hcursor = 0;
						int32_t vcursor = 0;
						int32_t next_line = 0;
						if(thm.toggle_button_t[selected_template].on_region.primary.bg != -1) {
							auto l = thm.toggle_button_t[selected_template].on_region.text_margin_left.resolve(float(16 * 8), float(2 * 8), 8.0f);
							auto t = thm.toggle_button_t[selected_template].on_region.text_margin_top.resolve(float(16 * 8), float(2 * 8), 8.0f);
							auto r = thm.toggle_button_t[selected_template].on_region.text_margin_right.resolve(float(16 * 8), float(2 * 8), 8.0f);
							auto b = thm.toggle_button_t[selected_template].on_region.text_margin_bottom.resolve(float(16 * 8), float(2 * 8), 8.0f);

							render_asvg_rect(thm.backgrounds[thm.toggle_button_t[selected_template].on_region.primary.bg].renders, hcursor, vcursor, next_line, 16, 2, 8);

							if(0 <= thm.toggle_button_t[selected_template].on_region.primary.color && thm.toggle_button_t[selected_template].on_region.primary.color < int32_t(thm.colors.size())) {
								render_hollow_rect(
									color3f{
										thm.colors[thm.toggle_button_t[selected_template].on_region.primary.color].r,
										thm.colors[thm.toggle_button_t[selected_template].on_region.primary.color].g,
										thm.colors[thm.toggle_button_t[selected_template].on_region.primary.color].b },
										drag_offset_x + 0 + l * ui_scale,
										drag_offset_y + vcursor + t * ui_scale,
										std::max(1, int32_t((float(16 * 8) - (r + l))* ui_scale)),
										std::max(1, int32_t((float(2 * 8) - (t + b))* ui_scale)));
							} else {
								render_hollow_rect(color3f{ 1.f, 0.f, 0.f },
									drag_offset_x + 0 + l * ui_scale,
									drag_offset_y + vcursor + t * ui_scale,
									std::max(1, int32_t((float(16 * 8) - (r + l))* ui_scale)),
									std::max(1, int32_t((float(2 * 8) - (t + b))* ui_scale)));
							}
						}
						vcursor = next_line;
						hcursor = 0;
						if(thm.toggle_button_t[selected_template].on_region.active.bg != -1) {
							render_asvg_rect(thm.backgrounds[thm.toggle_button_t[selected_template].on_region.active.bg].renders, hcursor, vcursor, next_line, 16, 2, 8);
						}
						vcursor = next_line;
						hcursor = 0;
						if(thm.toggle_button_t[selected_template].on_region.disabled.bg != -1) {
							render_asvg_rect(thm.backgrounds[thm.toggle_button_t[selected_template].on_region.disabled.bg].renders, hcursor, vcursor, next_line, 16, 2, 8);
						}
						vcursor = next_line;
						hcursor = 0;

						if(thm.toggle_button_t[selected_template].off_region.primary.bg != -1) {
							auto l = thm.toggle_button_t[selected_template].off_region.text_margin_left.resolve(float(16 * 8), float(2 * 8), 8.0f);
							auto t = thm.toggle_button_t[selected_template].off_region.text_margin_top.resolve(float(16 * 8), float(2 * 8), 8.0f);
							auto r = thm.toggle_button_t[selected_template].off_region.text_margin_right.resolve(float(16 * 8), float(2 * 8), 8.0f);
							auto b = thm.toggle_button_t[selected_template].off_region.text_margin_bottom.resolve(float(16 * 8), float(2 * 8), 8.0f);

							render_asvg_rect(thm.backgrounds[thm.toggle_button_t[selected_template].off_region.primary.bg].renders, hcursor, vcursor, next_line, 16, 2, 8);

							if(0 <= thm.toggle_button_t[selected_template].off_region.primary.color && thm.toggle_button_t[selected_template].off_region.primary.color < int32_t(thm.colors.size())) {
								render_hollow_rect(
									color3f{
										thm.colors[thm.toggle_button_t[selected_template].off_region.primary.color].r,
										thm.colors[thm.toggle_button_t[selected_template].off_region.primary.color].g,
										thm.colors[thm.toggle_button_t[selected_template].off_region.primary.color].b },
										drag_offset_x + 0 + l * ui_scale,
										drag_offset_y + vcursor + t * ui_scale,
										std::max(1, int32_t((float(16 * 8) - (r + l))* ui_scale)),
										std::max(1, int32_t((float(2 * 8) - (t + b))* ui_scale)));
							} else {
								render_hollow_rect(color3f{ 1.f, 0.f, 0.f },
									drag_offset_x + 0 + l * ui_scale,
									drag_offset_y + vcursor + t * ui_scale,
									std::max(1, int32_t((float(16 * 8) - (r + l))* ui_scale)),
									std::max(1, int32_t((float(2 * 8) - (t + b))* ui_scale)));
							}
						}
						vcursor = next_line;
						hcursor = 0;
						if(thm.toggle_button_t[selected_template].off_region.active.bg != -1) {
							render_asvg_rect(thm.backgrounds[thm.toggle_button_t[selected_template].off_region.active.bg].renders, hcursor, vcursor, next_line, 16, 2, 8);
						}
						vcursor = next_line;
						hcursor = 0;
						if(thm.toggle_button_t[selected_template].off_region.disabled.bg != -1) {
							render_asvg_rect(thm.backgrounds[thm.toggle_button_t[selected_template].off_region.disabled.bg].renders, hcursor, vcursor, next_line, 16, 2, 8);
						}
						vcursor = next_line;
						hcursor = 0;
					}
					break;
				case template_project::template_type::label:
					if(0 <= selected_template && selected_template < int32_t(thm.label_t.size())) {
						int32_t hcursor = 0;
						int32_t vcursor = 0;
						int32_t next_line = 0;
						if(thm.label_t[selected_template].primary.bg != -1) {
							render_asvg_rect(thm.backgrounds[thm.label_t[selected_template].primary.bg].renders, hcursor, vcursor, next_line, 20, 2, 8);
						}
					}
					break;
				case template_project::template_type::layout_region:
					if(0 <= selected_template && selected_template < int32_t(thm.layout_region_t.size())) {
						int32_t hcursor = 0;
						int32_t vcursor = 0;
						int32_t next_line = 0;
						if(thm.layout_region_t[selected_template].bg != -1) {
							render_asvg_rect(thm.backgrounds[thm.layout_region_t[selected_template].bg].renders, hcursor, vcursor, next_line, 25, 25, 8);
						}
						hcursor = int32_t((25 * 4 - 8 * 5) * ui_scale);
						vcursor = int32_t((22 * 8) * ui_scale);
						if(thm.layout_region_t[selected_template].left_button != -1) {
							auto b = thm.layout_region_t[selected_template].left_button;

							auto l = thm.iconic_button_t[b].primary.icon_left.resolve(float(2 * 8), float(2 * 8), 8.0f) * ui_scale + hcursor;
							auto t = thm.iconic_button_t[b].primary.icon_top.resolve(float(2 * 8), float(2 * 8), 8.0f) * ui_scale + vcursor;
							auto r = thm.iconic_button_t[b].primary.icon_right.resolve(float(2 * 8), float(2 * 8), 8.0f) * ui_scale + hcursor;
							auto bm = thm.iconic_button_t[b].primary.icon_bottom.resolve(float(2 * 8), float(2 * 8), 8.0f) * ui_scale + vcursor;

							render_asvg_rect(thm.backgrounds[thm.iconic_button_t[b].primary.bg].renders, hcursor, vcursor, next_line, 2, 2, 8);

							hcursor = l;
							vcursor = t;

							if(thm.layout_region_t[selected_template].left_button_icon != -1) {
								render_svg_rect(thm.icons[thm.layout_region_t[selected_template].left_button_icon].renders,
									hcursor, vcursor, next_line, int32_t((r - l) / ui_scale), int32_t((bm - t) / ui_scale),
									thm.colors[thm.iconic_button_t[thm.layout_region_t[selected_template].left_button_icon].primary.icon_color]);
							}
						}
						hcursor = int32_t((25 * 4 + 8 * 3) * ui_scale);
						vcursor = int32_t((22 * 8) * ui_scale);
						if(thm.layout_region_t[selected_template].right_button != -1) {
							auto b = thm.layout_region_t[selected_template].left_button;

							auto l = thm.iconic_button_t[b].primary.icon_left.resolve(float(2 * 8), float(2 * 8), 8.0f) * ui_scale + hcursor;
							auto t = thm.iconic_button_t[b].primary.icon_top.resolve(float(2 * 8), float(2 * 8), 8.0f) * ui_scale + vcursor;
							auto r = thm.iconic_button_t[b].primary.icon_right.resolve(float(2 * 8), float(2 * 8), 8.0f) * ui_scale + hcursor;
							auto bm = thm.iconic_button_t[b].primary.icon_bottom.resolve(float(2 * 8), float(2 * 8), 8.0f) * ui_scale + vcursor;

							render_asvg_rect(thm.backgrounds[thm.iconic_button_t[b].primary.bg].renders, hcursor, vcursor, next_line, 2, 2, 8);

							hcursor = l;
							vcursor = t;

							if(thm.layout_region_t[selected_template].right_button_icon != -1) {
								render_svg_rect(thm.icons[thm.layout_region_t[selected_template].right_button_icon].renders,
									hcursor, vcursor, next_line, int32_t((r - l) / ui_scale), int32_t((bm - t) / ui_scale),
									thm.colors[thm.iconic_button_t[b].primary.icon_color]);
							}
						}
						vcursor = next_line;
						hcursor = 0;

						// TODO: sub controls
					}
					break;
				case template_project::template_type::progress_bar:
					if(0 <= selected_template && selected_template < int32_t(thm.progress_bar_t.size())) {
						int32_t hcursor = 0;
						int32_t vcursor = 0;
						int32_t next_line = 0;
						if(thm.progress_bar_t[selected_template].bg_a != -1) {
							render_asvg_rect(thm.backgrounds[thm.progress_bar_t[selected_template].bg_a].renders, hcursor, vcursor, next_line, 12, 3, 8);
						}
						vcursor = next_line;
						hcursor = 0;
						if(thm.progress_bar_t[selected_template].bg_b != -1) {
							render_asvg_rect(thm.backgrounds[thm.progress_bar_t[selected_template].bg_b].renders, hcursor, vcursor, next_line, 12, 3, 8);
						}
					}
					break;
				case template_project::template_type::window:
					if(0 <= selected_template && selected_template < int32_t(thm.window_t.size())) {
						int32_t hcursor = 0;
						int32_t vcursor = 0;
						int32_t next_line = 0;
						if(thm.window_t[selected_template].bg != -1) {
							render_asvg_rect(thm.backgrounds[thm.window_t[selected_template].bg].renders, hcursor, vcursor, next_line, 50, 50, 8);
						}
						vcursor = int32_t((0 + thm.window_t[selected_template].v_close_button_margin * 8) * ui_scale);
						hcursor = int32_t((400 - thm.window_t[selected_template].h_close_button_margin * 8 - 24) * ui_scale);
						if(thm.window_t[selected_template].close_button_definition != -1) {
							auto l = thm.iconic_button_t[thm.window_t[selected_template].close_button_definition].primary.icon_left.resolve(float(3 * 8), float(3 * 8), 8.0f) * ui_scale + hcursor;
							auto t = thm.iconic_button_t[thm.window_t[selected_template].close_button_definition].primary.icon_top.resolve(float(3 * 8), float(3 * 8), 8.0f) * ui_scale + vcursor;
							auto r = thm.iconic_button_t[thm.window_t[selected_template].close_button_definition].primary.icon_right.resolve(float(3 * 8), float(3 * 8), 8.0f) * ui_scale + hcursor;
							auto b = thm.iconic_button_t[thm.window_t[selected_template].close_button_definition].primary.icon_bottom.resolve(float(3 * 8), float(3 * 8), 8.0f) * ui_scale + vcursor;

							if(thm.iconic_button_t[thm.window_t[selected_template].close_button_definition].primary.bg != -1)
								render_asvg_rect(thm.backgrounds[thm.iconic_button_t[thm.window_t[selected_template].close_button_definition].primary.bg].renders, hcursor, vcursor, next_line, 3, 3, 8);

							hcursor = l;
							vcursor = t;

							if(thm.window_t[selected_template].close_button_icon != -1) {
								render_svg_rect(thm.icons[thm.window_t[selected_template].close_button_icon].renders,
									hcursor, vcursor, next_line, int32_t((r - l) / ui_scale), int32_t((b - t) / ui_scale),
									thm.colors[thm.iconic_button_t[thm.window_t[selected_template].close_button_definition].primary.icon_color]);
							}
						}
						if(thm.window_t[selected_template].layout_region_definition != -1) {
							vcursor = int32_t(20 * ui_scale);
							hcursor = int32_t(20 * ui_scale);

							if(thm.layout_region_t[thm.window_t[selected_template].layout_region_definition].bg != -1) {
								render_asvg_rect(
									thm.backgrounds[thm.layout_region_t[thm.window_t[selected_template].layout_region_definition].bg].renders,
									hcursor, vcursor, next_line, 25, 25, 8);
							}
						}
					}
					break;
				default:
					break;
			}
		

		//const int32_t render_grid_scale = 128;
		//render_textured_rect(color3f{ 0.f, 0.f, 0.f }, drag_offset_x, drag_offset_y,
		//	std::max(1, int32_t(8 * render_grid_scale * ui_scale)), std::max(1, int32_t(2 * render_grid_scale * ui_scale)),
		//	test_rendered_svg.get_render(4000, 1000, render_grid_scale, 1.0f));
		//render_textured_rect(color3f{ 0.f, 0.f, 0.f }, (drag_offset_x + int32_t(8 * render_grid_scale * ui_scale)), drag_offset_y,
		//	std::max(1, int32_t(16 * render_grid_scale * ui_scale)), std::max(1, int32_t(3 * render_grid_scale * ui_scale)),
		//	test_rendered_svg.get_render(8000, 1500, render_grid_scale, 2.0f));

		glDepthRange(-1.0f, 1.0f);

		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);
	}

	// Cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();

	CoUninitialize();
	return 0;
}
