#include <icy_engine/graphics/icy_render_core.hpp>
#include <icy_engine/core/icy_json.hpp>

using namespace icy;

ICY_STATIC_NAMESPACE_BEG

static const auto key_time = "time"_s;
static const auto key_value = "value"_s;

static const auto key_world = "world"_s;
static const auto key_scale = "scale"_s;
static const auto key_angle = "angle"_s;
static const auto key_prefix = "prefix"_s;
static const auto key_suffix = "suffix"_s;
static const auto key_fps = "fps"_s;
static const auto key_frames= "frames"_s;
static const auto key_nodes = "nodes"_s;
static const auto key_texture_type = "textureType"_s;
static const auto key_map_type = "mapType"_s;
static const auto key_map_mode = "mapMode"_s;
static const auto key_compress = "compress"_s;
static const auto key_blend = "blend"_s;
static const auto key_width = "width"_s;
static const auto key_height = "height"_s;
static const auto key_bytes = "bytes"_s;
static const auto key_index = "index"_s;
static const auto key_oper = "oper"_s;

static const auto key_shading = "shading"_s;
static const auto key_two_sided = "twoSided"_s;
static const auto key_opacity = "opacity"_s;
static const auto key_transparency = "transparency"_s;
static const auto key_bump_scaling = "bumpScaling"_s;
static const auto key_reflectivity = "reflectivity"_s;
static const auto key_shininess = "shininess"_s;
static const auto key_shininess_percent = "shininessPercent"_s;
static const auto key_refract = "refract"_s;
static const auto key_color_diffuse = "colorDiffuse"_s;
static const auto key_color_ambient = "colorAmbient"_s;
static const auto key_color_specular = "colorSpecular"_s;
static const auto key_color_emissive = "colorEmissive"_s;
static const auto key_color_transparent = "colorTransparent"_s;
static const auto key_color_reflective = "colorReflective"_s;
static const auto key_textures = "textures"_s;

static const auto key_vertex = "vertex"_s;
static const auto key_name = "name"_s;
static const auto key_weights = "weights"_s;
static const auto key_transform = "transform"_s;
static const auto key_uv_count = "uvCount"_s;

static const auto key_tex = "tex"_s;
static const auto key_color = "color"_s;
static const auto key_mesh_type = "meshType"_s;
static const auto key_indices = "indices"_s;
static const auto key_channels = "channels"_s;
static const auto key_bones = "bones"_s;
static const auto key_material = "material"_s;

static const auto key_light_type = "lightType"_s;
static const auto key_factor_const = "factorConst"_s;
static const auto key_factor_linear = "factorLinear"_s;
static const auto key_factor_quad = "factorQuad"_s;
static const auto key_angle_inner = "angleInner"_s;
static const auto key_angle_outer = "angleOuter"_s;

static const auto key_fov = "fov"_s;
static const auto key_zmin = "zMin"_s;
static const auto key_zmax = "zMax"_s;
static const auto key_aspect = "aspect"_s;

static const auto key_parent = "parent"_s;
static const auto key_meshes = "meshes"_s;
static const auto key_camera = "camera"_s;
static const auto key_light = "light"_s;

template<typename T>
static error_type find_enum(const string_view input, T& output) noexcept
{
	if (input.empty())
		return error_type();

	for (auto k = 0_z; k < size_t(T::_total); ++k)
	{
		if (to_string(T(k)) == input)
		{
			output = T(k);
			return error_type();
		}
	}
	return make_stdlib_error(std::errc::invalid_argument);
}

ICY_STATIC_NAMESPACE_END


string_view icy::to_string(const render_animation_type input) noexcept
{
	switch (input)
	{
	case render_animation_type::first: return "first"_s;
	case render_animation_type::linear: return "linear"_s;
	case render_animation_type::repeat: return "repeat"_s;
	}
	return string_view();
}
string_view icy::to_string(const render_light_type input) noexcept
{
	switch (input)
	{
	case render_light_type::dir: return "directional"_s;
	case render_light_type::point: return "point"_s;
	case render_light_type::spot: return "spot"_s;
	case render_light_type::ambient: return "ambient"_s;
	case render_light_type::area: return "area"_s;
	}
	return string_view();
}
string_view icy::to_string(const render_texture_type input) noexcept
{
	switch (input)
	{
	case render_texture_type::diffuse: return "diffuse"_s;
	case render_texture_type::specular: return "specular"_s;
	case render_texture_type::ambient: return "ambient"_s;
	case render_texture_type::emissive: return "emissive"_s;
	case render_texture_type::height: return "height"_s;
	case render_texture_type::normals: return "normals"_s;
	case render_texture_type::shininess: return "shininess"_s;
	case render_texture_type::opacity: return "opacity"_s;
	case render_texture_type::displacement: return "displacement"_s;
	case render_texture_type::lightmap: return "lightmap"_s;
	case render_texture_type::reflection: return "reflection"_s;
	case render_texture_type::pbr_base_color: return "pbrBaseColor"_s;
	case render_texture_type::pbr_normal_camera: return "pbrNormalCamera"_s;
	case render_texture_type::pbr_emission_color: return "pbrEmissionColor"_s;
	case render_texture_type::pbr_metalness: return "pbrMetalness"_s;
	case render_texture_type::pbr_diffuse_roughness: return "pbrDiffuseRoughness"_s;
	case render_texture_type::pbr_ambient_occlusion: return "pbrAmbientOcclusion"_s;
	}
	return string_view();
}
string_view icy::to_string(const render_shading_model input) noexcept
{
	switch (input)
	{
	case render_shading_model::wire: return "wireFrame"_s;
	case render_shading_model::flat: return "flat"_s;
	case render_shading_model::gouraud: return "gouraud"_s;
	case render_shading_model::phong: return "phong"_s;
	case render_shading_model::blinn: return "blinn"_s;
	case render_shading_model::comic: return "comic"_s;
	case render_shading_model::oren_nayar: return "orenNayar"_s;
	case render_shading_model::minnaert: return "minnaert"_s;
	case render_shading_model::cook_torrance: return "cookTorrance"_s;
	case render_shading_model::fresnel: return "fresnel"_s;
	}
	return string_view();
}
string_view icy::to_string(const render_texture_map_type input) noexcept
{
	switch (input)
	{
	case render_texture_map_type::uv: return "uv"_s;
	case render_texture_map_type::sphere: return "sphere"_s;
	case render_texture_map_type::cylinder: return "cylinder"_s;
	case render_texture_map_type::box: return "box"_s;
	case render_texture_map_type::plane: return "plane"_s;
	}
	return string_view();
}
string_view icy::to_string(const render_texture_map_mode input) noexcept
{
	switch (input)
	{
	case render_texture_map_mode::wrap: return "wrap"_s;
	case render_texture_map_mode::clamp: return "clamp"_s;
	case render_texture_map_mode::decal: return "decal"_s;
	case render_texture_map_mode::mirror: return "mirror"_s;
	}
	return string_view();
}
string_view icy::to_string(const render_texture_oper input) noexcept
{
	switch (input)
	{
	case render_texture_oper::multiply: return "mul"_s;
	case render_texture_oper::add: return "add"_s;
	case render_texture_oper::substract: return "sub"_s;
	case render_texture_oper::divide: return "div"_s;
	case render_texture_oper::smooth_add: return "smoothAdd"_s;
	case render_texture_oper::signed_add: return "signedAdd"_s;
	}
	return string_view();
}
string_view icy::to_string(const render_texture_compress input) noexcept
{
	switch (input)
	{
	case render_texture_compress::dds1: return "dds1"_s;
	case render_texture_compress::dds5: return "dds5"_s;
	}
	return string_view();
}
string_view icy::to_string(const render_mesh_type input) noexcept
{
	switch (input)
	{
	case render_mesh_type::point: return "point"_s;
	case render_mesh_type::line: return "line"_s;
	case render_mesh_type::triangle: return "triangle"_s;
	case render_mesh_type::polygon: return "polygon"_s;
	}
	return string_view();
}

error_type icy::to_value(const json& input, render_vec2& output) noexcept
{
	if (input.type() != json_type::array)
		return make_stdlib_error(std::errc::invalid_argument);
	
	if (input.size() != 2)
		return make_stdlib_error(std::errc::invalid_argument);

	ICY_ERROR(input.at(0)->get(output.x));
	ICY_ERROR(input.at(1)->get(output.y));
	return error_type();
}
error_type icy::to_value(const json& input, render_vec3& output) noexcept
{
	if (input.type() != json_type::array)
		return make_stdlib_error(std::errc::invalid_argument);

	if (input.size() != 3)
		return make_stdlib_error(std::errc::invalid_argument);

	ICY_ERROR(input.at(0)->get(output.x));
	ICY_ERROR(input.at(1)->get(output.y));
	ICY_ERROR(input.at(2)->get(output.z));
	return error_type();
}
error_type icy::to_value(const json& input, render_vec4& output) noexcept
{
	if (input.type() != json_type::array)
		return make_stdlib_error(std::errc::invalid_argument);

	ICY_ERROR(input.at(0)->get(output.x));
	ICY_ERROR(input.at(1)->get(output.y));
	ICY_ERROR(input.at(2)->get(output.z));
	ICY_ERROR(input.at(3)->get(output.w));
	return error_type();
}
error_type icy::to_value(const json& input, render_transform& output) noexcept
{
	if (input.type() != json_type::object)
		return make_stdlib_error(std::errc::invalid_argument);

	if (const auto world = input.find(key_world))
	{
		ICY_ERROR(to_value(*world, output.world));
	}
	if (const auto scale = input.find(key_scale))
	{
		ICY_ERROR(to_value(*scale, output.scale));
	}
	if (const auto angle = input.find(key_angle))
	{
		ICY_ERROR(to_value(*angle, output.angle));
	}
	return error_type();
}
error_type icy::to_json(const render_vec2& input, json& output) noexcept
{
	output = json_type::array;
	ICY_ERROR(output.push_back(input.x));
	ICY_ERROR(output.push_back(input.y));
	return error_type();
}
error_type icy::to_json(const render_vec3& input, json& output) noexcept
{
	output = json_type::array;
	ICY_ERROR(output.push_back(input.x));
	ICY_ERROR(output.push_back(input.y));
	ICY_ERROR(output.push_back(input.z));
	return error_type();
}
error_type icy::to_json(const render_vec4& input, json& output) noexcept
{
	output = json_type::array;
	ICY_ERROR(output.push_back(input.x));
	ICY_ERROR(output.push_back(input.y));
	ICY_ERROR(output.push_back(input.z));
	ICY_ERROR(output.push_back(input.w));
	return error_type();
}
error_type icy::to_json(const render_transform& input, json& output) noexcept
{
	output = json_type::object;
	if (input.world.x || input.world.y || input.world.z)
	{
		json jworld;
		ICY_ERROR(to_json(input.world, jworld));
		ICY_ERROR(output.insert(key_world, std::move(jworld)));
	}
	if (input.scale.x != 1 || input.scale.y != 1 || input.scale.z != 1)
	{
		json jscale;
		ICY_ERROR(to_json(input.scale, jscale));
		ICY_ERROR(output.insert(key_scale, std::move(jscale)));
	}
	if (input.angle.x != 0 || input.angle.y != 0 || input.angle.z != 0 || input.angle.w != 1)
	{
		json jangle;
		ICY_ERROR(to_json(input.angle, jangle));
		ICY_ERROR(output.insert(key_angle, std::move(jangle)));
	}
	return error_type();
}

error_type icy::to_value(const json& input, render_animation_node::step3& output) noexcept
{
	if (input.type() != json_type::object)
		return make_stdlib_error(std::errc::invalid_argument);

	auto time = 0.0;
	ICY_ERROR(input.get(key_time, output.time));
	if (const auto value = input.find(key_value))
	{
		ICY_ERROR(to_value(*value, output.value));
	}
	return error_type();
}
error_type icy::to_value(const json& input, render_animation_node::step4& output) noexcept
{
	if (input.type() != json_type::object)
		return make_stdlib_error(std::errc::invalid_argument);

	auto time = 0.0;
	ICY_ERROR(input.get(key_time, output.time));
	if (const auto value = input.find(key_value))
	{
		ICY_ERROR(to_value(*value, output.value));
	}
	return error_type();
}
error_type icy::to_value(const json& input, render_animation_node& output) noexcept
{
	if (input.type() != json_type::object)
		return make_stdlib_error(std::errc::invalid_argument);

	const auto world = input.find(key_world);
	if (world && world->type() == json_type::array)
	{
		for (auto k = 0u; k < world->size(); ++k)
		{
			render_animation_node::step3 new_step;
			ICY_ERROR(to_value(*world->at(k), new_step));
			ICY_ERROR(output.world.push_back(std::move(new_step)));
		}
	}
	const auto scale = input.find(key_scale);
	if (scale && scale->type() == json_type::array)
	{
		for (auto k = 0u; k < scale->size(); ++k)
		{
			render_animation_node::step3 new_step;
			ICY_ERROR(to_value(*scale->at(k), new_step));
			ICY_ERROR(output.scale.push_back(std::move(new_step)));
		}
	}
	const auto angle = input.find(key_angle);
	if (angle && angle->type() == json_type::array)
	{
		for (auto k = 0u; k < angle->size(); ++k)
		{
			render_animation_node::step4 new_step;
			ICY_ERROR(to_value(*angle->at(k), new_step));
			ICY_ERROR(output.angle.push_back(std::move(new_step)));
		}
	}

	ICY_ERROR(find_enum(input.get(key_prefix), output.prefix));
	ICY_ERROR(find_enum(input.get(key_suffix), output.suffix));

	return error_type();
}
error_type icy::to_value(const json& input, render_animation& output) noexcept
{
	if (input.type() != json_type::object)
		return make_stdlib_error(std::errc::invalid_argument);

	ICY_ERROR(input.get(key_fps, output.fps));
	ICY_ERROR(input.get(key_frames, output.frames));
	
	const auto nodes = input.find(key_nodes);
	if (nodes && nodes->type() == json_type::object)
	{
		for (auto k = 0u; k < nodes->size(); ++k)
		{
			const auto& key = nodes->keys()[k];
			const auto& val = nodes->vals()[k];
			
			auto node_key = guid();
			auto node_val = render_animation_node();
			ICY_ERROR(to_value(key, node_key));
			ICY_ERROR(to_value(val, node_val));
			ICY_ERROR(output.nodes.insert(node_key, std::move(node_val)));
		}
	}
	
	return error_type();
}
error_type icy::to_value(const json& input, render_texture& output) noexcept
{
	if (input.type() != json_type::object)
		return make_stdlib_error(std::errc::invalid_argument);

	ICY_ERROR(input.get(key_blend, output.blend));
	ICY_ERROR(find_enum(input.get(key_map_type), output.map_type));
	const auto map_mode = input.find(key_map_mode);
	if (map_mode && map_mode->type() == json_type::array)
	{
		for (auto k = 0u; k < std::min(3_z, map_mode->size()); ++k)
		{
			ICY_ERROR(find_enum(map_mode->at(k)->get(), output.map_mode[k]));
		}
	}
	ICY_ERROR(find_enum(input.get(key_texture_type), output.type));
	ICY_ERROR(input.get(key_width, output.size.x));
	ICY_ERROR(input.get(key_height, output.size.y));

	const string_view str = input.get(key_bytes);
	ICY_ERROR(output.bytes.resize(base64_decode_size(str.ubytes().size())));
	ICY_ERROR(base64_decode(str, array_view<uint8_t>(output.bytes)));
	return error_type();
}
error_type icy::to_value(const json& input, render_material::tuple& output) noexcept
{
	if (input.type() != json_type::object)
		return make_stdlib_error(std::errc::invalid_argument);

	ICY_ERROR(input.get(key_index, output.index));
	ICY_ERROR(find_enum(input.get(key_oper), output.oper));
	ICY_ERROR(find_enum(input.get(key_texture_type), output.type));
	return error_type();
}
error_type icy::to_value(const json& input, render_material& output) noexcept
{
	if (input.type() != json_type::object)
		return make_stdlib_error(std::errc::invalid_argument);

	ICY_ERROR(find_enum(input.get(key_shading), output.shading));
	input.get(key_two_sided, output.two_sided);
	input.get(key_opacity, output.opacity);
	input.get(key_transparency, output.transparency);
	input.get(key_bump_scaling, output.bump_scaling);
	input.get(key_reflectivity, output.reflectivity);
	input.get(key_shininess, output.shininess);
	input.get(key_shininess_percent, output.shininess_percent);
	input.get(key_refract, output.refract);
	input.get(key_color_diffuse, output.color_diffuse);
	input.get(key_color_ambient, output.color_ambient);
	input.get(key_color_specular, output.color_specular);
	input.get(key_color_emissive, output.color_emissive);
	input.get(key_color_transparent, output.color_transparent);
	input.get(key_color_reflective, output.color_reflective);

	const auto textures = input.find(key_textures);
	if (textures && textures->type() == json_type::array)
	{
		for (auto k = 0u; k < textures->size(); ++k)
		{
			render_material::tuple new_tuple;
			ICY_ERROR(to_value(*textures->at(k), new_tuple));
			ICY_ERROR(output.textures.push_back(std::move(new_tuple)));
		}
	}
	return error_type();
}
error_type icy::to_value(const json& input, render_bone::weight& output) noexcept
{
	if (input.type() != json_type::object)
		return make_stdlib_error(std::errc::invalid_argument);

	ICY_ERROR(input.get(key_value, output.value));
	ICY_ERROR(input.get(key_vertex, output.vertex));
	return error_type();
}
error_type icy::to_value(const json& input, render_bone& output) noexcept
{
	if (input.type() != json_type::object)
		return make_stdlib_error(std::errc::invalid_argument);

	input.get(key_name, output.name);
	if (const auto transform = input.find(key_transform))
	{
		ICY_ERROR(to_value(*transform, output.transform));
	}
	const auto weights = input.find(key_weights);
	if (weights && weights->type() == json_type::array)
	{
		for (auto k = 0u; k < weights->size(); ++k)
		{
			render_bone::weight weight;
			ICY_ERROR(to_value(*weights->at(k), weight));
			ICY_ERROR(output.weights.push_back(weight));
		}
	}

	return error_type();
}
error_type icy::to_value(const json& input, render_mesh::channel& output) noexcept
{
	if (input.type() != json_type::object)
		return make_stdlib_error(std::errc::invalid_argument);

	const auto tex = input.find(key_tex);
	const auto col = input.find(key_color);
	if (tex && tex->type() == json_type::array)
	{
		for (auto n = 0u; n < tex->size(); ++n)
		{
			render_vec2 vec;
			ICY_ERROR(to_value(*tex->at(n), vec));
			ICY_ERROR(output.tex.push_back(vec));
		}
	}
	if (col && col->type() == json_type::array)
	{
		for (auto n = 0u; n < col->size(); ++n)
		{
			color color;
			ICY_ERROR(to_value(*col->at(n), color));
			ICY_ERROR(output.color.push_back(color));
		}
	}
	return error_type();
}
error_type icy::to_value(const json& input, render_mesh& output) noexcept
{
	if (input.type() != json_type::object)
		return make_stdlib_error(std::errc::invalid_argument);

	ICY_ERROR(find_enum(input.get(key_mesh_type), output.type));

	const auto world = input.find(key_world);
	if (world && world->type() == json_type::array)
	{
		for (auto k = 0u; k < world->size(); ++k)
		{
			render_vec3 vec;
			ICY_ERROR(to_value(*world->at(k), vec));
			ICY_ERROR(output.world.push_back(vec));
		}
	}
	const auto angle = input.find(key_angle);
	if (angle && angle->type() == json_type::array)
	{
		for (auto k = 0u; k < angle->size(); ++k)
		{
			render_vec4 vec;
			ICY_ERROR(to_value(*angle->at(k), vec));
			ICY_ERROR(output.angle.push_back(vec));
		}
	}
	const auto bones = input.find(key_bones);
	if (bones && bones->type() == json_type::array)
	{
		for (auto k = 0u; k < bones->size(); ++k)
		{
			render_bone bone;
			ICY_ERROR(to_value(*bones->at(k), bone));
			ICY_ERROR(output.bones.push_back(std::move(bone)));
		}
	}
	input.get(key_material, output.material);

	const auto indices = input.find(key_indices);
	if (indices && indices->type() == json_type::array)
	{
		for (auto k = 0u; k < indices->size(); ++k)
		{
			auto index = 0u;
			ICY_ERROR(indices->at(k)->get(index));
			ICY_ERROR(output.indices.push_back(index));
		}
	}
	const auto channels = input.find(key_channels);
	if (channels && channels->type() == json_type::array)
	{
		for (auto k = 0u; k < std::min(render_channel_count, channels->size()); ++k)
			ICY_ERROR(to_value(*channels->at(k), output.channels[k]));
	}
	return error_type();
}
error_type icy::to_value(const json& input, render_light& output) noexcept
{
	if (input.type() != json_type::object)
		return make_stdlib_error(std::errc::invalid_argument);

	ICY_ERROR(find_enum(input.get(key_light_type), output.type));
	input.get(key_width, output.size.x);
	input.get(key_height, output.size.y);
	if (const auto world = input.find(key_world))
		ICY_ERROR(to_value(*world, output.world));
	if (const auto angle = input.find(key_angle))
		ICY_ERROR(to_value(*angle, output.angle));

	input.get(key_factor_const, output.factor_const);
	input.get(key_factor_linear, output.factor_linear);
	input.get(key_factor_quad, output.factor_quad);
	input.get(key_color_diffuse, output.diffuse);
	input.get(key_color_specular, output.specular);
	input.get(key_color_ambient, output.ambient);
	input.get(key_angle_inner, output.angle_inner);
	input.get(key_angle_outer, output.angle_outer);

	return error_type();
}
error_type icy::to_value(const json& input, render_camera& output) noexcept
{
	if (input.type() != json_type::object)
		return make_stdlib_error(std::errc::invalid_argument);

	if (const auto world = input.find(key_world))
		ICY_ERROR(to_value(*world, output.world));
	if (const auto angle = input.find(key_angle))
		ICY_ERROR(to_value(*angle, output.angle));

	ICY_ERROR(input.get(key_fov, output.fov));
	ICY_ERROR(input.get(key_zmin, output.zmin));
	ICY_ERROR(input.get(key_zmax, output.zmax));
	ICY_ERROR(input.get(key_aspect, output.aspect));

	return error_type();
}
error_type icy::to_value(const json& input, render_node& output) noexcept
{
	if (input.type() != json_type::object)
		return make_stdlib_error(std::errc::invalid_argument);

	if (const auto transform = input.find(key_transform))
		ICY_ERROR(to_value(*transform, output.transform));

	const auto nodes = input.find(key_nodes);
	if (nodes && nodes->type() == json_type::array)
	{
		for (auto k = 0u; k < nodes->size(); ++k)
		{
			guid node;
			ICY_ERROR(to_value(nodes->at(k)->get(), node));
			ICY_ERROR(output.nodes.push_back(node));
		}
	}
	const auto meshes = input.find(key_meshes);
	if (meshes && meshes->type() == json_type::array)
	{
		for (auto k = 0u; k < meshes->size(); ++k)
		{
			guid mesh;
			ICY_ERROR(to_value(meshes->at(k)->get(), mesh));
			ICY_ERROR(output.meshes.push_back(mesh));
		}
	}

	if (const auto camera = input.find(key_camera))
	{
		render_camera new_camera;
		ICY_ERROR(to_value(*camera, new_camera));
		ICY_ERROR(make_unique(std::move(new_camera), output.camera));
	}
	if (const auto light = input.find(key_light))
	{
		render_light new_light;
		ICY_ERROR(to_value(*light, new_light));
		ICY_ERROR(make_unique(std::move(new_light), output.light));
	}
	return error_type();
}

error_type icy::to_json(const render_animation_node::step3& input, json& output) noexcept
{
	ICY_ERROR(output.insert(key_time, input.time));
	json value;
	ICY_ERROR(to_json(input.value, value));
	ICY_ERROR(output.insert(key_value, std::move(value)));
	return error_type();
}
error_type icy::to_json(const render_animation_node::step4& input, json& output) noexcept
{
	ICY_ERROR(output.insert(key_time, input.time));
	json value;
	ICY_ERROR(to_json(input.value, value));
	ICY_ERROR(output.insert(key_value, std::move(value)));
	return error_type();
}
error_type icy::to_json(const render_animation_node& input, json& output) noexcept
{
	output = json_type::object;

	json world = json_type::array;
	for (auto&& istep : input.world)
	{
		json jstep;
		ICY_ERROR(to_json(istep, jstep));
		ICY_ERROR(world.push_back(std::move(jstep)));
	}
	ICY_ERROR(output.insert(key_world, std::move(world)));

	json scale = json_type::array;
	for (auto&& istep : input.scale)
	{
		json jstep;
		ICY_ERROR(to_json(istep, jstep));
		ICY_ERROR(scale.push_back(std::move(jstep)));
	}
	ICY_ERROR(output.insert(key_scale, std::move(scale)));

	json angle = json_type::array;
	for (auto&& istep : input.angle)
	{
		json jstep;
		ICY_ERROR(to_json(istep, jstep));
		ICY_ERROR(angle.push_back(std::move(jstep)));
	}
	ICY_ERROR(output.insert(key_angle, std::move(angle)));

	ICY_ERROR(output.insert(key_prefix, to_string(input.prefix)));
	ICY_ERROR(output.insert(key_suffix, to_string(input.suffix)));

	return error_type();
}
error_type icy::to_json(const render_animation& input, json& output) noexcept
{
	output = json_type::object;
	ICY_ERROR(output.insert(key_fps, input.fps));
	ICY_ERROR(output.insert(key_frames, input.frames));
	for (auto&& pair : input.nodes)
	{
		string str_key;
		ICY_ERROR(to_string(pair.key, str_key));
		json json_val;
		ICY_ERROR(to_json(pair.value, json_val));
		ICY_ERROR(output.insert(std::move(str_key), std::move(json_val)));
	}	
	return error_type();
}
error_type icy::to_json(const render_texture& input, json& output) noexcept
{
	output = json_type::object;
	ICY_ERROR(output.insert(key_blend, input.blend));
	ICY_ERROR(output.insert(key_compress, to_string(input.compress)));
	json map_mode = json_type::array;
	for (auto&& val : input.map_mode)
		ICY_ERROR(map_mode.push_back(to_string(val)));
	ICY_ERROR(output.insert(key_map_mode, std::move(map_mode)));
	ICY_ERROR(output.insert(key_map_type, to_string(input.map_type)));
	ICY_ERROR(output.insert(key_width, input.size.x));
	ICY_ERROR(output.insert(key_height, input.size.y));
	ICY_ERROR(output.insert(key_texture_type, to_string(input.type)));

	array<char> bytes;
	ICY_ERROR(bytes.resize(base64_encode_size(input.bytes.size())));
	ICY_ERROR(base64_encode(const_array_view<uint8_t>(input.bytes), array_view<char>(bytes)));
	string str;
	ICY_ERROR(to_string(bytes, str));
	ICY_ERROR(output.insert(key_bytes, std::move(str)));
	return error_type();
}
error_type icy::to_json(const render_material::tuple& input, json& output) noexcept
{
	output = json_type::object;
	ICY_ERROR(output.insert(key_index, input.index));
	ICY_ERROR(output.insert(key_oper, to_string(input.oper)));
	ICY_ERROR(output.insert(key_texture_type, to_string(input.type)));
	return error_type();
}
error_type icy::to_json(const render_material& input, json& output) noexcept
{
	output = json_type::object;
	ICY_ERROR(output.insert(key_shading, to_string(input.shading)));
	ICY_ERROR(output.insert(key_two_sided, input.two_sided));
	ICY_ERROR(output.insert(key_opacity, input.opacity));
	ICY_ERROR(output.insert(key_transparency, input.transparency));
	ICY_ERROR(output.insert(key_bump_scaling, input.bump_scaling));
	ICY_ERROR(output.insert(key_reflectivity, input.reflectivity));
	ICY_ERROR(output.insert(key_shininess, input.shininess));
	ICY_ERROR(output.insert(key_shininess_percent, input.shininess_percent));
	ICY_ERROR(output.insert(key_refract, input.refract));

	json color_diffuse;
	json color_ambient;
	json color_specular;
	json color_emissive;
	json color_transparent;
	json color_reflective;
	ICY_ERROR(to_json(input.color_diffuse, color_diffuse));
	ICY_ERROR(to_json(input.color_ambient, color_ambient));
	ICY_ERROR(to_json(input.color_specular, color_specular));
	ICY_ERROR(to_json(input.color_emissive, color_emissive));
	ICY_ERROR(to_json(input.color_transparent, color_transparent));
	ICY_ERROR(to_json(input.color_reflective, color_reflective));
	ICY_ERROR(output.insert(key_color_diffuse, std::move(color_diffuse)));
	ICY_ERROR(output.insert(key_color_ambient, std::move(color_ambient)));
	ICY_ERROR(output.insert(key_color_specular, std::move(color_specular)));
	ICY_ERROR(output.insert(key_color_emissive, std::move(color_emissive)));
	ICY_ERROR(output.insert(key_color_transparent, std::move(color_transparent)));
	ICY_ERROR(output.insert(key_color_reflective, std::move(color_reflective)));

	json textures = json_type::array;
	for (auto&& texture : input.textures)
	{
		json new_texture;
		ICY_ERROR(to_json(texture, new_texture));
		ICY_ERROR(textures.push_back(std::move(new_texture)));
	}
	ICY_ERROR(output.insert(key_textures, std::move(textures)));

	return error_type();
}
error_type icy::to_json(const render_bone::weight& input, json& output) noexcept
{
	output = json_type::object;
	ICY_ERROR(output.insert(key_value, input.value));
	ICY_ERROR(output.insert(key_vertex, input.vertex));
	return error_type();
}
error_type icy::to_json(const render_bone& input, json& output) noexcept
{
	output = json_type::object;
	ICY_ERROR(output.insert(key_name, input.name));
	json transform;
	ICY_ERROR(to_json(input.transform, transform));
	ICY_ERROR(output.insert(key_transform, std::move(transform)));

	json weights = json_type::array;
	for (auto&& weight : input.weights)
	{
		json new_weight;
		ICY_ERROR(to_json(weight, new_weight));
		ICY_ERROR(weights.push_back(std::move(new_weight)));
	}
	ICY_ERROR(output.insert(key_weights, std::move(weights)));
	return error_type();
}
error_type icy::to_json(const render_mesh::channel& input, json& output) noexcept
{
	output = json_type::object;
	json colors = json_type::array;
	for (auto&& color : input.color)
	{
		json new_color;
		ICY_ERROR(to_json(color, new_color));
		ICY_ERROR(colors.push_back(std::move(new_color)));
	}
	ICY_ERROR(output.insert(key_color, std::move(colors)));
	json texs = json_type::array;
	for (auto&& tex : input.tex)
	{
		json new_tex;
		ICY_ERROR(to_json(tex, new_tex));
		ICY_ERROR(texs.push_back(std::move(new_tex)));
	}
	ICY_ERROR(output.insert(key_tex, std::move(texs)));
	return error_type();
}
error_type icy::to_json(const render_mesh& input, json& output) noexcept
{
	output = json_type::object;

	json worlds = json_type::array;
	for (auto&& world : input.world)
	{
		json new_world;
		ICY_ERROR(to_json(world, new_world));
		ICY_ERROR(worlds.push_back(std::move(new_world)));
	}
	ICY_ERROR(output.insert(key_world, std::move(worlds)));

	json angles = json_type::array;
	for (auto&& angle : input.angle)
	{
		json new_angle;
		ICY_ERROR(to_json(angle, new_angle));
		ICY_ERROR(angles.push_back(std::move(new_angle)));
	}
	ICY_ERROR(output.insert(key_angle, std::move(angles)));

	json bones = json_type::array;
	for (auto&& bone : input.bones)
	{
		json new_bone;
		ICY_ERROR(to_json(bone, new_bone));
		ICY_ERROR(bones.push_back(std::move(new_bone)));
	}
	ICY_ERROR(output.insert(key_bones, std::move(bones)));

	json indices = json_type::array;
	for (auto&& index : input.indices)
		ICY_ERROR(indices.push_back(index));
	ICY_ERROR(output.insert(key_indices, std::move(indices)));
	ICY_ERROR(output.insert(key_material, input.material));
	ICY_ERROR(output.insert(key_mesh_type, to_string(input.type)));

	json channels = json_type::array;
	for (auto&& channel : input.channels)
	{
		if (channel.tex.empty() && channel.color.empty())
			break;
		json new_channel;
		ICY_ERROR(to_json(channel, new_channel));
		ICY_ERROR(channels.push_back(std::move(new_channel)));
	}
	return error_type();
}
error_type icy::to_json(const render_light& input, json& output) noexcept
{
	output = json_type::object;

	json world;
	json angle;
	json diffuse;
	json specular;
	json ambient;
	ICY_ERROR(to_json(input.world, world));
	ICY_ERROR(to_json(input.angle, angle));
	ICY_ERROR(to_json(input.diffuse, diffuse));
	ICY_ERROR(to_json(input.specular, specular));
	ICY_ERROR(to_json(input.ambient, ambient));

	ICY_ERROR(output.insert(key_light_type, to_string(input.type)));
	ICY_ERROR(output.insert(key_width, input.size.x));
	ICY_ERROR(output.insert(key_height, input.size.y));
	ICY_ERROR(output.insert(key_world, std::move(world)));
	ICY_ERROR(output.insert(key_angle, std::move(angle)));
	ICY_ERROR(output.insert(key_color_diffuse, std::move(diffuse)));
	ICY_ERROR(output.insert(key_color_specular, std::move(specular)));
	ICY_ERROR(output.insert(key_color_ambient, std::move(ambient)));
	ICY_ERROR(output.insert(key_factor_const, input.factor_const));
	ICY_ERROR(output.insert(key_factor_linear, input.factor_linear));
	ICY_ERROR(output.insert(key_factor_quad, input.factor_quad));
	ICY_ERROR(output.insert(key_angle_inner, input.angle_inner));
	ICY_ERROR(output.insert(key_angle_outer, input.angle_outer));
	return error_type();
}
error_type icy::to_json(const render_camera& input, json& output) noexcept
{
	output = json_type::object;

	json world;
	json angle;
	ICY_ERROR(to_json(input.world, world));
	ICY_ERROR(to_json(input.angle, angle));

	ICY_ERROR(output.insert(key_fov, input.fov));
	ICY_ERROR(output.insert(key_zmin, input.zmin));
	ICY_ERROR(output.insert(key_zmax, input.zmax));
	ICY_ERROR(output.insert(key_aspect, input.aspect));
	ICY_ERROR(output.insert(key_world, std::move(world)));
	ICY_ERROR(output.insert(key_angle, std::move(angle)));

	return error_type();
}
error_type icy::to_json(const render_node& input, json& output) noexcept
{
	output = json_type::object;

	json transform;
	ICY_ERROR(to_json(input.transform, transform));
	
	ICY_ERROR(output.insert(key_parent, input.parent));
	ICY_ERROR(output.insert(key_transform, std::move(transform)));

	if (!input.nodes.empty())
	{
		json nodes = json_type::array;
		for (auto&& node : input.nodes)
		{
			string new_node;
			ICY_ERROR(to_string(node, new_node));
			ICY_ERROR(nodes.push_back(std::move(new_node)));
		}
		ICY_ERROR(output.insert(key_nodes, std::move(nodes)));
	}

	if (!input.meshes.empty())
	{
		json meshes = json_type::array;
		for (auto&& mesh : input.meshes)
		{
			string new_mesh;
			ICY_ERROR(to_string(mesh, new_mesh));
			ICY_ERROR(meshes.push_back(std::move(new_mesh)));
		}
		ICY_ERROR(output.insert(key_meshes, std::move(meshes)));
	}

	if (input.camera)
	{
		json camera;
		ICY_ERROR(to_json(*input.camera, camera));
		ICY_ERROR(output.insert(key_camera, std::move(camera)));
	}
	if (input.light)
	{
		json light;
		ICY_ERROR(to_json(*input.light, light));
		ICY_ERROR(output.insert(key_light, std::move(light)));
	}

	return error_type();
}