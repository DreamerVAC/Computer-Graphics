#include "TRRenderer.h"

#include "glm/gtc/matrix_transform.hpp"

#include "TRShaderPipeline.h"
#include <cmath>

namespace TinyRenderer
{

	TRRenderer::TRRenderer(int width, int height)
		: m_backBuffer(nullptr), m_frontBuffer(nullptr)
	{
		//Double buffer to avoid flickering
		m_backBuffer = std::make_shared<TRFrameBuffer>(width, height);
		m_frontBuffer = std::make_shared<TRFrameBuffer>(width, height);

		//Setup viewport matrix (ndc space -> screen space)
		m_viewportMatrix = calcViewPortMatrix(width, height);
	}

	void TRRenderer::loadDrawableMesh(const std::string &filename)
	{
		TRDrawableMesh drawable;
		drawable.loadMeshFromFile(filename);
		m_drawableMeshes.push_back(drawable);
	}

	void TRRenderer::unloadDrawableMesh()
	{
		for (size_t i = 0; i < m_drawableMeshes.size(); ++i)
		{
			m_drawableMeshes[i].unload();
		}
		std::vector<TRDrawableMesh>().swap(m_drawableMeshes);
	}

	void TRRenderer::setViewMatrix(const glm::mat4 &view)
	{
		m_mvp_dirty = true;
		m_viewMatrix = view;
	}

	void TRRenderer::setModelMatrix(const glm::mat4 &model)
	{
		m_mvp_dirty = true;
		m_modelMatrix = model;
	}

	void TRRenderer::setProjectMatrix(const glm::mat4 &project)
	{
		m_mvp_dirty = true;
		m_projectMatrix = project;
	}

	void TRRenderer::setShaderPipeline(TRShaderPipeline::ptr shader)
	{
		m_shader_handler = shader;
	}

	glm::mat4 TRRenderer::getMVPMatrix()
	{
		if (m_mvp_dirty)
		{
			m_mvp_matrix = m_projectMatrix * m_viewMatrix * m_modelMatrix;
			m_mvp_dirty = false;
		}
		return m_mvp_matrix;
	}

	void TRRenderer::clearColor(glm::vec4 color)
	{
		m_backBuffer->clear(color);
	}

	void TRRenderer::renderAllDrawableMeshes()
	{
		if (m_shader_handler == nullptr)
		{
			m_shader_handler = std::make_shared<TRDefaultShaderPipeline>();
		}
		
		//Load the matrices
		m_shader_handler->setModelMatrix(m_modelMatrix);
		m_shader_handler->setViewProjectMatrix(m_projectMatrix * m_viewMatrix);

		//Draw a mesh step by step
		for (size_t m = 0; m < m_drawableMeshes.size(); ++m)
		{
			const auto& vertices = m_drawableMeshes[m].getVerticesAttrib();
			const auto& faces = m_drawableMeshes[m].getMeshFaces();
			for (size_t f = 0; f < faces.size(); ++f)
			{
				//A triangle as primitive, primitive assemably
				TRShaderPipeline::VertexData v[3];
				{
					v[0].pos = vertices.vpositions[faces[f].vposIndex[0]];
					v[0].col = glm::vec3(vertices.vcolors[faces[f].vposIndex[0]]);
					v[0].nor = vertices.vnormals[faces[f].vnorIndex[0]];
					v[0].tex = vertices.vtexcoords[faces[f].vtexIndex[0]];

					v[1].pos = vertices.vpositions[faces[f].vposIndex[1]];
					v[1].col = glm::vec3(vertices.vcolors[faces[f].vposIndex[1]]);
					v[1].nor = vertices.vnormals[faces[f].vnorIndex[1]];
					v[1].tex = vertices.vtexcoords[faces[f].vtexIndex[1]];

					v[2].pos = vertices.vpositions[faces[f].vposIndex[2]];
					v[2].col = glm::vec3(vertices.vcolors[faces[f].vposIndex[2]]);
					v[2].nor = vertices.vnormals[faces[f].vnorIndex[2]];
					v[2].tex = vertices.vtexcoords[faces[f].vtexIndex[2]];
				}

				//Vertex shader
				{
					m_shader_handler->vertexShader(v[0]);
					m_shader_handler->vertexShader(v[1]);
					m_shader_handler->vertexShader(v[2]);

				}

				//Perspective division & Backface culling
				{

					//Perspective correction before rasterization
					{
						TRShaderPipeline::VertexData::prePerspCorrection(v[0]);
						TRShaderPipeline::VertexData::prePerspCorrection(v[1]);
						TRShaderPipeline::VertexData::prePerspCorrection(v[2]);
					}

					//From clip space -> ndc space
					v[0].cpos /= v[0].cpos.w;
					v[1].cpos /= v[1].cpos.w;
					v[2].cpos /= v[2].cpos.w;

					if (isTowardBackFace(v[0].cpos, v[1].cpos, v[2].cpos))
					{
						continue;
					}
				}

				//Transform to screen space & Rasterization
				std::vector<TRShaderPipeline::VertexData> rasterized_points;
				{
					v[0].spos = glm::ivec2(m_viewportMatrix * v[0].cpos);
					v[1].spos = glm::ivec2(m_viewportMatrix * v[1].cpos);
					v[2].spos = glm::ivec2(m_viewportMatrix * v[2].cpos);
					rasterized_points = m_shader_handler->rasterize_wire(v[0], v[1], v[2], 
						m_backBuffer->getWidth(), m_backBuffer->getHeight());
				}

				//Fragment shader & Depth testing
				{
					for (auto &points : rasterized_points)
					{
						if (m_backBuffer->readDepth(points.spos.x, points.spos.y) > points.cpos.z)
						{
							//Perspective correction after rasterization
							TRShaderPipeline::VertexData::aftPrespCorrection(points);
							glm::vec4 fragColor;
							m_shader_handler->fragmentShader(points, fragColor);
							m_backBuffer->writeColor(points.spos.x, points.spos.y, fragColor);
							m_backBuffer->writeDepth(points.spos.x, points.spos.y, points.cpos.z);
						}
					}
				}
			}

		}

		//Swap double buffers
		{
			std::swap(m_backBuffer, m_frontBuffer);
		}
		
	}

	unsigned char* TRRenderer::commitRenderedColorBuffer()
	{
		return m_frontBuffer->getColorBuffer();
	}
	bool TRRenderer::isTowardBackFace(const glm::vec4 &v0, const glm::vec4 &v1, const glm::vec4 &v2) const
	{
		glm::vec3 edge1(v1.x - v0.x, v1.y - v0.y, v1.z - v0.z);
		glm::vec3 edge2(v2.x - v0.x, v2.y - v0.y, v2.z - v0.z);

		glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));
		glm::vec3 view = glm::vec3(0, 0, 1);

		return glm::dot(normal, view) < 0;
	}

	glm::mat4 TRRenderer::calcViewMatrix(glm::vec3 camera, glm::vec3 target, glm::vec3 worldUp)
	{
		//Setup view matrix (world space -> camera space)
		glm::mat4 vMat = glm::mat4(1.0f);

		//Implement the calculation of view matrix, and then set it to vMat
		//  Note: You can use any glm function (such as glm::normalize, glm::cross, glm::dot) except glm::lookAt

		//zAxis
		glm::vec3 forward = glm::normalize(camera-target);
		//xAxis
		glm::vec3 right = glm::normalize(glm::cross(worldUp, forward));
		//yAxis
		glm::vec3 up = glm::cross(forward, right);

		vMat[0][0] = right.x;
		vMat[1][0] = right.y;
		vMat[2][0] = right.z;
		vMat[0][1] = up.x;
		vMat[1][1] = up.y;
		vMat[2][1] = up.z;
		vMat[0][2] = forward.x;
		vMat[1][2] = forward.y;
		vMat[2][2] = forward.z;
		vMat[3][0] = -glm::dot(right, camera);
		vMat[3][1] = -glm::dot(up, camera);
		vMat[3][2] = -glm::dot(forward, camera);
		vMat[0][3] = 0;
		vMat[1][3] = 0;
		vMat[2][3] = 0;
		vMat[3][3] = 1;

		return vMat;
	}

	glm::mat4 TRRenderer::calcPerspProjectMatrix(float fovy, float aspect, float near, float far)
	{
		//Setup perspective matrix (camera space -> clip space)
		glm::mat4 pMat = glm::mat4(1.0f);

		//Implement the calculation of perspective matrix, and then set it to pMat
		//  Note: You can use any math function (such as std::tan) except glm::perspective
		float tanHalfFovy = std::tan(fovy / 2.0f);

		pMat[0][0] = 1.0f / (aspect * tanHalfFovy);
		pMat[0][1] = 0;
		pMat[0][2] = 0;
		pMat[0][3] = 0;
		pMat[1][0] = 0;
		pMat[1][1] = 1.0f / tanHalfFovy;
		pMat[1][2] = 0;
		pMat[1][3] = 0;
		pMat[2][0] = 0;
		pMat[2][1] = 0;
		pMat[2][2] = (far + near) / (near-far);
		pMat[2][3] = -(2 * near - far) / (near - far);
		pMat[3][0] = 0;
		pMat[3][1] = 0;
		pMat[3][2] = -1.0f;
		pMat[3][3] = 0;

		return pMat;
	}

	glm::mat4 TRRenderer::calcViewPortMatrix(int width, int height)
	{
		//Setup viewport matrix (ndc space -> screen space)
		glm::mat4 vpMat = glm::mat4(1.0f);

		//Implement the calculation of viewport matrix, and then set it to vpMat
		vpMat[0][0] = static_cast<float>(width) / 2.0f;
		vpMat[1][0] = 0;
		vpMat[2][0] = 0;
		vpMat[3][0] = static_cast<float>(width) / 2.0f;

		vpMat[0][1] = 0;
		vpMat[1][1] = static_cast<float>(-height) / 2.0f;
		vpMat[2][1] = 0;
		vpMat[3][1] = static_cast<float>(height) / 2.0f;

		vpMat[0][2] = 0;
		vpMat[1][2] = 0;
		vpMat[2][2] = 0.5f;
		vpMat[3][2] = 0.5f;

		vpMat[0][3] = 0;
		vpMat[1][3] = 0;
		vpMat[2][3] = 0;
		vpMat[3][3] = 1.0f;

		return vpMat;
	}

	glm::mat4 TRRenderer::calcOrthoProjectMatrix(float left, float right, float bottom, float top, float near, float far)
	{
		//Setup orthogonal matrix (camera space -> homogeneous space)
		glm::mat4 pMat = glm::mat4(1.0f);

		//Implement the calculation of orthogonal projection, and then set it to pMat
		pMat[0][0] = 2 / (right - left);
		pMat[1][0] = 0;
		pMat[2][0] = 0;
		pMat[3][0] = -(right+left)/(right-left);
		pMat[0][1] = 0;
		pMat[1][1] = 2/(top-bottom);
		pMat[2][1] = 0;
		pMat[3][1] = -(top+bottom)/(top-bottom);
		pMat[0][2] = 0;
		pMat[1][2] = 0;
		pMat[2][2] = -2 / (far - near);
		pMat[3][2] = (near + far) / (near - far);
		pMat[0][3] = 0;
		pMat[1][3] = 0;
		pMat[2][3] = 0;
		pMat[3][3] = 1;



		return pMat;
	}

}