#include "raytracer_renderer.h"

#include "utils/resource_utils.h"

void cg::renderer::ray_tracing_renderer::init()
{
	render_target = std::make_shared<cg::resource<cg::unsigned_color>>(settings->width, settings->height);

	model = std::make_shared<cg::world::model>();
	model->load_obj(settings->model_path);

	raytracer =
		std::make_shared<cg::renderer::raytracer<cg::vertex, cg::unsigned_color>>();

	raytracer->set_render_target(render_target);
	raytracer->set_viewport(settings->width, settings->height);
	raytracer->set_per_shape_vertex_buffer(model->get_per_shape_buffer());

	shadow_raytracer = 
		std::make_shared<cg::renderer::raytracer<cg::vertex, cg::unsigned_color>>();

	//v - 0.24 1.98 0.16 v - 0.24 1.98 - 0.22 v 0.23 1.98 -
	//	0.22 v 0.23 1.98 0.16
	float3 color{ 0.78f, 0.78f, 0.78f };
	color /= 1;
	//lights.push_back({ float3{ 0, 1.58f, -0.03f }, color });
	/*lights.push_back({ float3{ -0.24f, 1.58f, 0.16f }, color });
	lights.push_back({ float3{ -0.24f, 1.58f, -0.22f }, color });
	lights.push_back({ float3{ 0.23f, 1.58f, -0.22f }, color });
	lights.push_back({ float3{ 0.23f, 1.58f, 0.16f }, color });*/

	// Set camera
	camera = std::make_shared<cg::world::camera>();
	camera->set_height(static_cast<float>(settings->height));
	camera->set_width(static_cast<float>(settings->width));
	camera->set_position(float3{ settings->camera_position[0],
								 settings->camera_position[1],
								 settings->camera_position[2] });
	camera->set_theta(settings->camera_theta);
	camera->set_phi(settings->camera_phi);

	camera->set_angle_of_view(settings->camera_angle_of_view);
	camera->set_z_far(settings->camera_z_far);
	camera->set_z_near(settings->camera_z_near);
}

void cg::renderer::ray_tracing_renderer::destroy() {}

void cg::renderer::ray_tracing_renderer::update() {}

void cg::renderer::ray_tracing_renderer::render()
{
	raytracer->clear_render_target({0, 0, 0});

	raytracer->miss_shader = [](const ray& ray) {
		cg::renderer::payload payload = {};
		payload.t = -1.f;
		//payload.color = { ray.direction.x / 0.5f + 0.5f, ray.direction.y / 0.5f + 0.5f,
		//				  ray.direction.z / 0.5f + 0.5f };
		payload.color = { 0.f, 0.f, 0.f };
		return payload;
	};

	raytracer->closest_hit_shader = [&](
			const ray& ray,
			payload& payload,
			const triangle<cg::vertex>& triangle,
			size_t depth
		) {
		float3 result_color = triangle.emissive;
		float3 position = ray.position + ray.direction * payload.t;
		float3 normal = payload.bary.x * triangle.na +
						payload.bary.y * triangle.nb +
						payload.bary.z * triangle.nc;
		//for (auto& light : lights)
		for (size_t i = 0; i < 2; i++)
		{
			float3 direction{ raytracer->get_random(omp_get_thread_num() + clock()),
							  raytracer->get_random(omp_get_thread_num() + clock()),
							  raytracer->get_random(omp_get_thread_num() + clock()) };
			//cg::renderer::ray to_light(position, light.position - position);
			cg::renderer::ray to_light(position, normal + direction);
			auto light_payload = raytracer->trace_ray(to_light, depth);
			//auto shadow_payload =
				//shadow_raytracer->trace_ray(to_light, 1, length(light.position - position));

			//if (shadow_payload.t == -1.f)
			{
				/*result_color += triangle.diffuse * light.color *
								std::max(dot(normal, to_light.direction), 0.f);*/
				result_color += triangle.diffuse * light_payload.color.to_float3() *
								std::max(dot(normal, to_light.direction), 0.f);
			}
		}
		payload.color = cg::color::from_float3(result_color);
		return payload;
	};

	raytracer->build_acceleration_structure();

	shadow_raytracer->acceleration_structures = raytracer->acceleration_structures;
	shadow_raytracer->miss_shader = [](const ray& ray) {
		cg::renderer::payload payload = {};
		payload.t = -1.f;
		return payload;
	};
	shadow_raytracer->any_hit_shader = [](const ray& ray, payload& payload, const triangle<cg::vertex>& triangle) {
		return payload;
	};

	for (unsigned frame_id = 0; frame_id < settings->accumulation_num; frame_id++)
	{
		raytracer->ray_generation(
			camera->get_position(), camera->get_direction(),
			camera->get_right(), camera->get_up(), 1.f / (frame_id + 1.f));

	}
	cg::utils::save_resource(*render_target, settings->result_path);
}