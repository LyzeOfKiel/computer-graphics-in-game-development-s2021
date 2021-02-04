#pragma once

#include "resource.h"

#include <functional>
#include <iostream>
#include <linalg.h>
#include <memory>


using namespace linalg::aliases;

namespace cg::renderer
{
template<typename VB, typename RT>
class rasterizer
{
public:
	rasterizer(){};
	~rasterizer(){};
	void set_render_target(
		std::shared_ptr<resource<RT>> in_render_target,
		std::shared_ptr<resource<float>> in_depth_buffer = nullptr);
	void clear_render_target(const RT& in_clear_value, const float in_depth = FLT_MAX);

	void set_vertex_buffer(std::shared_ptr<resource<VB>> in_vertex_buffer);

	void set_viewport(size_t in_width, size_t in_height);

	void draw(size_t num_vertexes, size_t vertex_offest);

	float4 get_equation_plane(
		float x1, float y1, float z1, float x2, float y2, float z2, float x3,
		float y3, float z3);


	std::function<std::pair<float4, VB>(float4 vertex, VB vertex_data)> vertex_shader;
	std::function<cg::color(const VB& vertex_data, const float z)> pixel_shader;

protected:
	std::shared_ptr<cg::resource<VB>> vertex_buffer;
	std::shared_ptr<cg::resource<RT>> render_target;
	std::shared_ptr<cg::resource<float>> depth_buffer;

	size_t width = 1920;
	size_t height = 1080;

	float edge_function(float2 a, float2 b, float2 c);
	bool depth_test(float z, size_t x, size_t y);
};

template<typename VB, typename RT>
inline void rasterizer<VB, RT>::set_render_target(
	std::shared_ptr<resource<RT>> in_render_target,
	std::shared_ptr<resource<float>> in_depth_buffer)
{
	if (in_render_target)
		render_target = in_render_target;
	if (in_depth_buffer)
		depth_buffer = in_depth_buffer;
}

template<typename VB, typename RT>
inline void rasterizer<VB, RT>::clear_render_target(const RT& in_clear_value, const float in_depth)
{
	if (render_target)
	{
		for (size_t i = 0; i < render_target->get_number_of_elements(); i++)
			render_target->item(i) = in_clear_value;
	}
	if (depth_buffer)
	{
		for (size_t i = 0; i < depth_buffer->get_number_of_elements(); i++)
			depth_buffer->item(i) = in_depth;
	}
}

template<typename VB, typename RT>
inline void rasterizer<VB, RT>::set_vertex_buffer(std::shared_ptr<resource<VB>> in_vertex_buffer)
{
	vertex_buffer = in_vertex_buffer;
}

template<typename VB, typename RT>
inline void rasterizer<VB, RT>::set_viewport(size_t in_width, size_t in_height)
{
	width = in_width;
	height = in_height;
}

template<typename VB, typename RT>
inline void rasterizer<VB, RT>::draw(size_t num_vertexes, size_t vertex_offest)
{
	size_t vertex_id = vertex_offest;
	while (vertex_id < vertex_offest + num_vertexes)
	{
		std::vector<VB> vertices(3);
		vertices[0] = vertex_buffer->item(vertex_id++);
		vertices[1] = vertex_buffer->item(vertex_id++);
		vertices[2] = vertex_buffer->item(vertex_id++);
		for (auto& v : vertices)
		{
			float4 coords{v.x, v.y, v.z, 1.f};
			auto processed_v = vertex_shader(coords, v);

			v.x = processed_v.first.x / processed_v.first.w;
			v.y = processed_v.first.y / processed_v.first.w;
			v.z = processed_v.first.z / processed_v.first.w;

			v.x = (v.x + 1.f) * width / 2.f;
			v.y = (-v.y + 1.f) * height/ 2.f;
		}

		float2 bounding_box_begin{
			std::clamp(
				std::min(std::min(vertices[0].x, vertices[1].x), vertices[2].x),
				0.f, static_cast<float>(width) - 1.f
			),
			std::clamp(
				std::min(std::min(vertices[0].y, vertices[1].y), vertices[2].y),
				0.f, static_cast<float>(height) - 1.f
			)
		};
		float2 bounding_box_end{
			std::clamp(
				std::max(std::max(vertices[0].x, vertices[1].x), vertices[2].x),
				0.f, static_cast<float>(width) - 1.f
			),
			std::clamp(
				std::max(std::max(vertices[0].y, vertices[1].y), vertices[2].y),
				0.f, static_cast<float>(height) - 1.f
			)
		};

		// 2 * s of triangle
		float2 a{ vertices[0].x, vertices[0].y };
		float2 b{ vertices[1].x, vertices[1].y };
		float2 c{ vertices[2].x, vertices[2].y };
		float edge = edge_function(a, b, c);

		for (int x = static_cast<int>(bounding_box_begin.x);
			 x <= static_cast<int>(bounding_box_end.x); x++)
		{
			for (int y = static_cast<int>(bounding_box_begin.y);
				 y <= static_cast<int>(bounding_box_end.y); y++)
			{
				// v2 bary
				float edge0 = edge_function(a, b, float2{ static_cast<float>(x), static_cast<float>(y) });
				// v0 bary
				float edge1 = edge_function(b, c, float2{ static_cast<float>(x), static_cast<float>(y) });
				// v1 bary
				float edge2 = edge_function(c, a, float2{ static_cast<float>(x), static_cast<float>(y) });

				if (edge0 >= 0.f && edge1 >= 0.f && edge2 >= 0.f)
				{
					float u = edge1 / edge;
					float v = edge2 / edge;
					float w = edge0 / edge;

					float4 plane = get_equation_plane(
						vertices[0].x, vertices[0].y, vertices[0].z,
						vertices[1].x, vertices[1].y, vertices[1].z,
						vertices[2].x, vertices[2].y, vertices[2].z);

					float z = (-plane.x * x - plane.y * y - plane.w) / plane.z;

					/*float z = 
						u * vertices[0].z +
						v * vertices[1].z +
						w * vertices[2].z;*/

					if (depth_test(z, x, y))
					{
						auto ps_res = pixel_shader(vertices[0], 0.f);
						render_target->item(x, y) = RT::from_color(ps_res);
						if (depth_buffer)
							depth_buffer->item(x, y) = z;
					}
				}
			}
		}
	}
}

template<typename VB, typename RT>
inline float rasterizer<VB, RT>::edge_function(float2 a, float2 b, float2 c)
{
	return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}

template<typename VB, typename RT>
float4 rasterizer<VB, RT>::get_equation_plane(
	float x1, float y1, float z1, float x2, float y2, float z2, float x3,
	float y3, float z3)
{
	float a1 = x2 - x1;
	float b1 = y2 - y1;
	float c1 = z2 - z1;
	float a2 = x3 - x1;
	float b2 = y3 - y1;
	float c2 = z3 - z1;
	float a = b1 * c2 - b2 * c1;
	float b = a2 * c1 - a1 * c2;
	float c = a1 * b2 - b1 * a2;
	float d = (-a * x1 - b * y1 - c * z1);
	return float4{ a, b, c, d };
} 

template<typename VB, typename RT>
inline bool rasterizer<VB, RT>::depth_test(float z, size_t x, size_t y)
{
	if (!depth_buffer)
		return true;
	return depth_buffer->item(x, y) > z;
}

} // namespace cg::renderer