#include <unordered_set>
#include <string>
#include <codecvt>
#include <algorithm>
#include <ifcpp/IFC4/include/IfcSurfaceStyle.h>
#include <ifcpp/IFC4/include/IfcBuildingStorey.h>
#include <ifcpp/IFC4/include/IfcGloballyUniqueId.h>
#include <ifcpp/IFC4/include/IfcLabel.h>
#include <ifcpp/IFC4/include/IfcObjectDefinition.h>
#include <ifcpp/IFC4/include/IfcProject.h>
#include <ifcpp/IFC4/include/IfcRelAggregates.h>
#include <ifcpp/IFC4/include/IfcRelAssociates.h>
#include <ifcpp/IFC4/include/IfcRelAssociatesMaterial.h>
#include <ifcpp/IFC4/include/IfcRelContainedInSpatialStructure.h>
#include <ifcpp/IFC4/include/IfcText.h>
#include <ifcpp/IFC4/include/IfcElement.h>
#include <ifcpp/model/BuildingModel.h>
#include <ifcpp/reader/ReaderSTEP.h>

class MyIfcTreeItem
{
public:
	MyIfcTreeItem() {}
	std::wstring m_name;
	std::wstring m_description;
	std::wstring m_entity_guid;
	std::string m_ifc_class_name;
	std::vector<shared_ptr<MyIfcTreeItem> > m_children;
};

shared_ptr<MyIfcTreeItem> resolveTreeItems(bool& contains_surface, shared_ptr<BuildingObject> obj, std::unordered_set<int>& set_visited)
{
	bool this_is_surface = false;
	bool child_contains_surface = false;

	shared_ptr<MyIfcTreeItem> item;
	shared_ptr<IfcObjectDefinition> obj_def = dynamic_pointer_cast<IfcObjectDefinition>(obj);
	auto element = dynamic_pointer_cast<IfcElement>(obj);
	if (element) {
		//this_is_surface = true;
		for (auto assWeak = element->m_HasAssociations_inverse.begin(); assWeak != element->m_HasAssociations_inverse.end(); assWeak++) {
			auto assMaterial = dynamic_pointer_cast<IfcRelAssociatesMaterial>(assWeak->lock());
			if (!assMaterial)
				continue;
			std::cout << "GOT MATERIAL ASSOCIATION!" << std::endl;
		}
	}

	if (obj_def)
	{
		if (set_visited.find(obj_def->m_entity_id) != set_visited.end())
		{
			return nullptr;
		}
		set_visited.insert(obj_def->m_entity_id);

		item = std::shared_ptr<MyIfcTreeItem>(new MyIfcTreeItem());
		item->m_ifc_class_name = obj_def->className();


		std::vector<std::pair<std::string, std::shared_ptr<BuildingObject>>> attr;
		obj_def->getAttributes(attr);
		for (auto aI = attr.begin(); aI != attr.end(); aI++)
		{
			auto surfaceAttr = dynamic_pointer_cast<IfcSurfaceStyle>(aI->second);
			if (surfaceAttr) {
				std::cout << "GOT AN ATTRIBUTE SURFACE!" << std::endl;
			}
		}

		// access some attributes of IfcObjectDefinition
		if (obj_def->m_GlobalId)
		{
			item->m_entity_guid = obj_def->m_GlobalId->m_value;
		}

		if (obj_def->m_Name)
		{
			item->m_name = obj_def->m_Name->m_value;
		}

		if (obj_def->m_Description)
		{
			item->m_description = obj_def->m_Description->m_value;
		}

		// check if there are child elements of current IfcObjectDefinition
		// child elements can be related to current IfcObjectDefinition either by IfcRelAggregates, or IfcRelContainedInSpatialStructure, see IFC doc
		if (obj_def->m_IsDecomposedBy_inverse.size() > 0)
		{
			// use inverse attribute to navigate to child elements:
			std::vector<weak_ptr<IfcRelAggregates> >& vec_IsDecomposedBy = obj_def->m_IsDecomposedBy_inverse;
			for (auto it = vec_IsDecomposedBy.begin(); it != vec_IsDecomposedBy.end(); ++it)
			{
				shared_ptr<IfcRelAggregates> rel_agg(*it);
				std::vector<shared_ptr<IfcObjectDefinition> >& vec_related_objects = rel_agg->m_RelatedObjects;
				for (shared_ptr<IfcObjectDefinition> child_obj_def : vec_related_objects)
				{
					shared_ptr<MyIfcTreeItem> child_tree_item = resolveTreeItems(child_contains_surface, child_obj_def, set_visited);
					if (child_tree_item)
					{
						item->m_children.push_back(child_tree_item);
					}
				}
			}
		}

		shared_ptr<IfcSpatialStructureElement> spatial_ele = dynamic_pointer_cast<IfcSpatialStructureElement>(obj_def);
		if (spatial_ele)
		{
			// use inverse attribute to navigate to child elements:
			std::vector<weak_ptr<IfcRelContainedInSpatialStructure> >& vec_contained = spatial_ele->m_ContainsElements_inverse;
			if (vec_contained.size() > 0)
			{
				for (auto it_rel_contained = vec_contained.begin(); it_rel_contained != vec_contained.end(); ++it_rel_contained)
				{
					shared_ptr<IfcRelContainedInSpatialStructure> rel_contained(*it_rel_contained);
					std::vector<shared_ptr<IfcProduct> >& vec_related_elements = rel_contained->m_RelatedElements;

					for (shared_ptr<IfcProduct> related_product : vec_related_elements)
					{
						shared_ptr<MyIfcTreeItem> child_tree_item = resolveTreeItems(child_contains_surface, related_product, set_visited);
						if (child_tree_item)
						{
							item->m_children.push_back(child_tree_item);
						}
					}
				}
			}
		}
	}


	if (this_is_surface || child_contains_surface) {
		contains_surface = true;
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		if (this_is_surface)
			std::cout << "Got surface - " << converter.to_bytes(item->m_description) << std::endl;
		if (child_contains_surface)
			std::cout << " - within " << converter.to_bytes(item->m_entity_guid) << " : " << converter.to_bytes(item->m_description) << std::endl;
	}

	return item;
}

int main(int argc, char* argv[])
{
	if (argc < 2)
		return 0;

	// 1: create an IFC model and a reader for IFC files in STEP format:
	shared_ptr<BuildingModel> ifc_model(new BuildingModel());
	shared_ptr<ReaderSTEP> step_reader(new ReaderSTEP());

	// 2: load the model:
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	step_reader->loadModelFromFile(converter.from_bytes(argv[1]), ifc_model);

	// 3: get a flat map of all loaded IFC entities:
	const std::map<int, shared_ptr<BuildingEntity> >& map_entities = ifc_model->getMapIfcEntities();

	for (auto it : map_entities)
	{
		shared_ptr<BuildingEntity> entity = it.second;

		// check for certain type of the entity:
		auto ifc_ss = dynamic_pointer_cast<IfcSurfaceStyle>(entity);
		if (ifc_ss)
		{
			std::wcout << L"found IfcSurfaceStyle entity with id: " << ifc_ss->m_entity_id << " and name: " << ifc_ss->m_Name->m_value << std::endl;
		}
	}

	// 4: traverse tree structure of model, starting at root object (IfcProject)
	shared_ptr<IfcProject> ifc_project = ifc_model->getIfcProject();
	std::unordered_set<int> set_visited;
	bool any_surfaces = false;
	shared_ptr<MyIfcTreeItem> root_item = resolveTreeItems(any_surfaces, ifc_project, set_visited);
	if (any_surfaces)
		std::cout << "Found at least one surface!" << std::endl;

	// you can access the model as a flat map (step 3), or a tree (step 4), depending on your requirements
	std::cout << "All done!" << std::endl;

	return 0;
}
