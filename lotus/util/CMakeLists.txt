target_sources(lotus-engine PUBLIC
	FILE_SET CXX_MODULES
	FILES
	async_queue.cppm
	id_generator.cppm
	random.cppm
	shared_linked_list.cppm
	task.cppm
	types.cppm
	util.cppm
	worker_pool.cppm
	worker_task.cppm
	PRIVATE
	worker_pool.cpp
)
