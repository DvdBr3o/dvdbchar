import("core.base.json")

function _auto_introduced_uniform_buffer(parameter, target)
	if parameter.type.elementVarLayout.bindings then
		for _, binding in pairs(parameter.type.elementVarLayout.bindings) do
			if binding.kind == "uniform" then
				target.bindings.uniform = {
					binding = target.bindings_count,
					kind = "uniform",
					offset = binding.offset,
					size = binding.size,
				}
				target.bindings_count = target.bindings_count + 1
				break
			end
		end
	elseif parameter.type.elementVarLayout.binding then
		local binding = parameter.type.elementVarLayout.binding
		if binding.kind == "uniform" then
			target.bindings.uniform = {
				binding = target.bindings_count,
				kind = "uniform",
				offset = binding.offset,
				size = binding.size,
			}
			target.bindings_count = target.bindings_count + 1
		end
	end

	target.entries = {}
	local binding_count = 0
	for _, field in pairs(parameter.type.elementVarLayout.type.fields) do
		target.entries[field.name] = {
			binding = binding_count,
			offset = field.binding.offset,
			size = field.binding.size,
		}
		binding_count = binding_count + 1
	end
end

function _descriptor_table_slots(parameter, target)
	for _, field in pairs(parameter.type.elementVarLayout.type.fields) do
		if field.type.kind == "resource" then
			local view_dimension
			if field.type.baseShape == "texture1D" then
				view_dimension = "e1D"
			elseif field.type.baseShape == "texture2D" then
				view_dimension = "e2D"
			elseif field.type.baseShape == "texture3D" then
				view_dimension = "e3D"
			end
			target.bindings[field.name] = {
				binding = target.bindings_count,
				kind = "texture",
				viewDimension = view_dimension,
			}
			target.bindings_count = target.bindings_count + 1
		elseif field.type.kind == "samplerState" then
			target.bindings[field.name] = {
				binding = target.bindings_count,
				kind = "sampler",
			}
			target.bindings_count = target.bindings_count + 1
		end
	end
end

function _nested_parameter_blocks(parameter, target)
	-- TODO:
end

function parse_layout(json_path)
	local refl = json.decode(io.readfile(json_path))
	local layout = {}
	layout.all = {}

	local layouts_binding_count = 0
	for _, parameter in pairs(refl.parameters) do
		table.insert(layout.all, {})
		local param = layout.all[#layout.all]
		param.name = parameter.name
		param.bindings = {}
		param.bindings_count = 0
		param.binding = layouts_binding_count
		layouts_binding_count = layouts_binding_count + 1

		_auto_introduced_uniform_buffer(parameter, param)
		_descriptor_table_slots(parameter, param)
		_nested_parameter_blocks(parameter, param)
	end

	layout.parameters = {}
	for _, p in ipairs(layout.all) do
		layout.parameters[p.name] = p
	end

	return json.encode(layout)
end
