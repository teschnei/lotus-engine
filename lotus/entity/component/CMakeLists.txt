target_sources(lotus-engine PUBLIC
	FILE_SET CXX_MODULES
	FILES
	animation.cppm
	camera_cascades.cppm
	camera.cppm
	component.cppm
	deformable_raster.cppm
	deformable_raytrace.cppm
	deformed_mesh.cppm
	instanced_models.cppm
	instanced_raster.cppm
	instanced_raytrace.cppm
	particle.cppm
	particle_raster.cppm
	particle_raytrace.cppm
	render_base.cppm
	static_collision.cppm
	PRIVATE
	camera.cpp
)
