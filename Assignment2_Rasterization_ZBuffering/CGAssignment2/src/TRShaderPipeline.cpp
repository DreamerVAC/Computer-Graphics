#include "TRShaderPipeline.h"

#include <algorithm>

namespace TinyRenderer
{
	//----------------------------------------------VertexData----------------------------------------------

	TRShaderPipeline::VertexData TRShaderPipeline::VertexData::lerp(
		const TRShaderPipeline::VertexData &v0,
		const TRShaderPipeline::VertexData &v1,
		float frac)
	{
		//Linear interpolation
		VertexData result;
		result.pos = (1.0f - frac) * v0.pos + frac * v1.pos;
		result.col = (1.0f - frac) * v0.col + frac * v1.col;
		result.nor = (1.0f - frac) * v0.nor + frac * v1.nor;
		result.tex = (1.0f - frac) * v0.tex + frac * v1.tex;
		result.cpos = (1.0f - frac) * v0.cpos + frac * v1.cpos;
		result.spos.x = (1.0f - frac) * v0.spos.x + frac * v1.spos.x;
		result.spos.y = (1.0f - frac) * v0.spos.y + frac * v1.spos.y;

		return result;
	}

	TRShaderPipeline::VertexData TRShaderPipeline::VertexData::barycentricLerp(
		const VertexData &v0, 
		const VertexData &v1, 
		const VertexData &v2,
		glm::vec3 w)
	{
		VertexData result;
		result.pos = w.x * v0.pos + w.y * v1.pos + w.z * v2.pos;
		result.col = w.x * v0.col + w.y * v1.col + w.z * v2.col;
		result.nor = w.x * v0.nor + w.y * v1.nor + w.z * v2.nor;
		result.tex = w.x * v0.tex + w.y * v1.tex + w.z * v2.tex;
		result.cpos = w.x * v0.cpos + w.y * v1.cpos + w.z * v2.cpos;
		result.spos.x = w.x * v0.spos.x + w.y * v1.spos.x + w.z * v2.spos.x;
		result.spos.y = w.x * v0.spos.y + w.y * v1.spos.y + w.z * v2.spos.y;

		return result;
	}

	void TRShaderPipeline::VertexData::prePerspCorrection(VertexData &v)
	{
		//Perspective correction: the world space properties should be multipy by 1/w before rasterization
		//https://zhuanlan.zhihu.com/p/144331875
		//We use pos.w to store 1/w
		v.pos.w = 1.0f / v.cpos.w;
		v.pos = glm::vec4(v.pos.x * v.pos.w, v.pos.y * v.pos.w, v.pos.z * v.pos.w, v.pos.w);
		v.tex = v.tex * v.pos.w;
		v.nor = v.nor * v.pos.w;
		v.col = v.col * v.pos.w;
	}

	void TRShaderPipeline::VertexData::aftPrespCorrection(VertexData &v)
	{
		//Perspective correction: the world space properties should be multipy by w after rasterization
		//https://zhuanlan.zhihu.com/p/144331875
		//We use pos.w to store 1/w
		float w = 1.0f / v.pos.w;
		v.pos = v.pos * w;
		v.tex = v.tex * w;
		v.nor = v.nor * w;
		v.col = v.col * w;
	}

	//----------------------------------------------TRShaderPipeline----------------------------------------------

	void TRShaderPipeline::rasterize_wire(
		const VertexData &v0,
		const VertexData &v1,
		const VertexData &v2,
		const unsigned int &screen_width,
		const unsigned int &screene_height,
		std::vector<VertexData> &rasterized_points)
	{
		//Draw each line step by step
		rasterize_wire_aux(v0, v1, screen_width, screene_height, rasterized_points);
		rasterize_wire_aux(v1, v2, screen_width, screene_height, rasterized_points);
		rasterize_wire_aux(v0, v2, screen_width, screene_height, rasterized_points);
	}

	void TRShaderPipeline::rasterize_fill_edge_function(
		const VertexData &v0,
		const VertexData &v1,
		const VertexData &v2,
		const unsigned int &screen_width,
		const unsigned int &screene_height,
		std::vector<VertexData> &rasterized_points)
	{
		//Edge-function rasterization algorithm

		// 2: Implement edge-function triangle rassterization algorithm
		// Note: You should use VertexData::barycentricLerp(v0, v1, v2, w) for interpolation, 
		//       interpolated points should be pushed back to rasterized_points.
		//       Interpolated points shold be discarded if they are outside the window. 

		//       v0.spos, v1.spos and v2.spos are the screen space vertices.

		int max_x = std::max({ v0.spos.x,v1.spos.x,v2.spos.x });
		int max_y = std::max({ v0.spos.y,v1.spos.y,v2.spos.y });
		int min_x = std::min({ v0.spos.x,v1.spos.x,v2.spos.x });
		int min_y = std::min({ v0.spos.y,v1.spos.y,v2.spos.y });
		for (int i = min_x; i <= max_x; i++) {
			for (int j = min_y; j <= max_y; j++) {
				glm::vec3 x = glm::vec3(v1.spos.x - v0.spos.x, v2.spos.x - v0.spos.x, v0.spos.x - i);
				glm::vec3 y = glm::vec3(v1.spos.y - v0.spos.y, v2.spos.y - v0.spos.y, v0.spos.y - j);
				glm::vec3 n = glm::cross(x, y);
				glm::vec3 w = { 1.0f - n.x / n.z - n.y / n.z,n.x / n.z,n.y / n.z };

				if (w.x >= 0 && w.y >= 0 && w.x + w.y <= 1) {
					VertexData p = VertexData::barycentricLerp(v0, v1, v2, w);
					p.spos.x = i;
					p.spos.y = j;
					rasterized_points.push_back(p);
				}
			}
		}
	}

	void TRShaderPipeline::rasterize_wire_aux(
		const VertexData &from,
		const VertexData &to,
		const unsigned int &screen_width,
		const unsigned int &screen_height,
		std::vector<VertexData> &rasterized_points)
	{

		//1: Implement Bresenham line rasterization
		// Note: You shold use VertexData::lerp(from, to, weight) for interpolation,
		//       interpolated points should be pushed back to rasterized_points.
		//       Interpolated points shold be discarded if they are outside the window. 
		
		//       from.spos and to.spos are the screen space vertices.

		int x0 = static_cast<int>(from.spos.x);
		int y0 = static_cast<int>(from.spos.y);
		int x1 = static_cast<int>(to.spos.x);
		int y1 = static_cast<int>(to.spos.y);

		int dx = abs(x1 - x0);
		int dy = abs(y1 - y0);

		int sx = x0 < x1 ? 1 : -1; // x 的前进方向
		int sy = y0 < y1 ? 1 : -1; // y 的前进方向

		// 判断斜率，决定主轴
		bool steep = dy > dx;

		if (steep) {
			// 交换 dx 和 dy
			std::swap(dx, dy);
			// 初始误差项
			int err = 2 * dx - dy;
			int x = x0;
			int y = y0;

			// 计算总步数用于插值
			int totalSteps = dy;
			int steps = 0;

			for (int i = 0; i <= dy; i++) {
				// 计算插值参数 t
				double t = totalSteps == 0 ? 0.0 : static_cast<double>(steps) / totalSteps;
				VertexData temp = VertexData::lerp(from, to, t);

				if (temp.spos.x >= 0 && temp.spos.x < screen_width && temp.spos.y >= 0 && temp.spos.y < screen_height) {
					rasterized_points.push_back(temp);
				}

				y += sy;
				if (err >= 0) {
					x += sx;
					err -= 2 * dy;
				}
				err += 2 * dx;
				steps++;
			}
		}
		else {
			// 初始误差项
			int err = 2 * dy - dx;
			int x = x0;
			int y = y0;

			// 计算总步数用于插值
			int totalSteps = dx;
			int steps = 0;

			for (int i = 0; i <= dx; i++) {
				// 计算插值参数 t
				double t = totalSteps == 0 ? 0.0 : static_cast<double>(steps) / totalSteps;
				VertexData temp = VertexData::lerp(from, to, t);

				if (temp.spos.x >= 0 && temp.spos.x < screen_width && temp.spos.y >= 0 && temp.spos.y < screen_height) {
					rasterized_points.push_back(temp);
				}

				x += sx;
				if (err >= 0) {
					y += sy;
					err -= 2 * dx;
				}
				err += 2 * dy;
				steps++;
			}
		}
	}

	void TRDefaultShaderPipeline::vertexShader(VertexData &vertex)
	{
		//Local space -> World space -> Camera space -> Project space
		vertex.pos = m_model_matrix * glm::vec4(vertex.pos.x, vertex.pos.y, vertex.pos.z, 1.0f);
		vertex.cpos = m_view_project_matrix * vertex.pos;
	}

	void TRDefaultShaderPipeline::fragmentShader(const VertexData &data, glm::vec4 &fragColor)
	{
		//Just return the color.
		fragColor = glm::vec4(data.tex, 0.0, 1.0f);
	}
}