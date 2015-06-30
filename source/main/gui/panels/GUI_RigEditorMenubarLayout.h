#pragma once

// ----------------------------------------------------------------------------
// Generated by MyGUI's LayoutEditor using RoR's code templates.
// Find the templates at [repository]/tools/MyGUI_LayoutEditor/
//
// IMPORTANT:
// Do not modify this code directly. Create a derived class instead.
// ----------------------------------------------------------------------------

#include "ForwardDeclarations.h"
#include "BaseLayout.h"

namespace RoR
{

namespace GUI
{
	
ATTRIBUTE_CLASS_LAYOUT(RigEditorMenubarLayout, "rig_editor_menubar.layout");
class RigEditorMenubarLayout : public wraps::BaseLayout
{

public:

	RigEditorMenubarLayout(MyGUI::Widget* _parent = nullptr);
	virtual ~RigEditorMenubarLayout();

protected:

	//%LE Widget_Declaration list start
	ATTRIBUTE_FIELD_WIDGET_NAME(RigEditorMenubarLayout, m_rig_editor_menubar, "rig_editor_menubar");
	MyGUI::MenuBar* m_rig_editor_menubar;

	ATTRIBUTE_FIELD_WIDGET_NAME(RigEditorMenubarLayout, m_menubar_item_file, "menubar_item_file");
	MyGUI::MenuItem* m_menubar_item_file;

	ATTRIBUTE_FIELD_WIDGET_NAME(RigEditorMenubarLayout, m_file_popup, "file_popup");
	MyGUI::PopupMenu* m_file_popup;

	ATTRIBUTE_FIELD_WIDGET_NAME(RigEditorMenubarLayout, m_file_popup_item_create_empty, "file_popup_item_create_empty");
	MyGUI::MenuItem* m_file_popup_item_create_empty;

	ATTRIBUTE_FIELD_WIDGET_NAME(RigEditorMenubarLayout, m_file_popup_item_open_json, "file_popup_item_open_json");
	MyGUI::MenuItem* m_file_popup_item_open_json;

	ATTRIBUTE_FIELD_WIDGET_NAME(RigEditorMenubarLayout, m_file_popup_item_save_json_as, "file_popup_item_save_json_as");
	MyGUI::MenuItem* m_file_popup_item_save_json_as;

	ATTRIBUTE_FIELD_WIDGET_NAME(RigEditorMenubarLayout, m_file_popup_item_import_truckfile, "file_popup_item_import_truckfile");
	MyGUI::MenuItem* m_file_popup_item_import_truckfile;

	ATTRIBUTE_FIELD_WIDGET_NAME(RigEditorMenubarLayout, m_file_popup_item_export_truckfile_as, "file_popup_item_export_truckfile_as");
	MyGUI::MenuItem* m_file_popup_item_export_truckfile_as;

	ATTRIBUTE_FIELD_WIDGET_NAME(RigEditorMenubarLayout, m_file_popup_item_close, "file_popup_item_close");
	MyGUI::MenuItem* m_file_popup_item_close;

	ATTRIBUTE_FIELD_WIDGET_NAME(RigEditorMenubarLayout, m_file_popup_item_quit, "file_popup_item_quit");
	MyGUI::MenuItem* m_file_popup_item_quit;

	ATTRIBUTE_FIELD_WIDGET_NAME(RigEditorMenubarLayout, m_menubar_item_project, "menubar_item_project");
	MyGUI::MenuItem* m_menubar_item_project;

	ATTRIBUTE_FIELD_WIDGET_NAME(RigEditorMenubarLayout, m_project_popup, "project_popup");
	MyGUI::PopupMenu* m_project_popup;

	ATTRIBUTE_FIELD_WIDGET_NAME(RigEditorMenubarLayout, m_project_popup_item_properties, "project_popup_item_properties");
	MyGUI::MenuItem* m_project_popup_item_properties;

	ATTRIBUTE_FIELD_WIDGET_NAME(RigEditorMenubarLayout, m_project_popup_item_land_properties, "project_popup_item_land_properties");
	MyGUI::MenuItem* m_project_popup_item_land_properties;

	ATTRIBUTE_FIELD_WIDGET_NAME(RigEditorMenubarLayout, m_project_popup_item_wheels, "project_popup_item_wheels");
	MyGUI::MenuItem* m_project_popup_item_wheels;

	ATTRIBUTE_FIELD_WIDGET_NAME(RigEditorMenubarLayout, m_project_popup_item_flares, "project_popup_item_flares");
	MyGUI::MenuItem* m_project_popup_item_flares;

	ATTRIBUTE_FIELD_WIDGET_NAME(RigEditorMenubarLayout, m_menubar_item_editor, "menubar_item_editor");
	MyGUI::MenuItem* m_menubar_item_editor;

	ATTRIBUTE_FIELD_WIDGET_NAME(RigEditorMenubarLayout, m_editor_popup, "editor_popup");
	MyGUI::PopupMenu* m_editor_popup;

	ATTRIBUTE_FIELD_WIDGET_NAME(RigEditorMenubarLayout, m_editor_popup_item_background_images, "editor_popup_item_background_images");
	MyGUI::MenuItem* m_editor_popup_item_background_images;

	ATTRIBUTE_FIELD_WIDGET_NAME(RigEditorMenubarLayout, m_editor_popup_item_mirror, "editor_popup_item_mirror");
	MyGUI::MenuItem* m_editor_popup_item_mirror;

	ATTRIBUTE_FIELD_WIDGET_NAME(RigEditorMenubarLayout, m_editor_popup_item_settings, "editor_popup_item_settings");
	MyGUI::MenuItem* m_editor_popup_item_settings;

	ATTRIBUTE_FIELD_WIDGET_NAME(RigEditorMenubarLayout, m_menubar_item_help, "menubar_item_help");
	MyGUI::MenuItem* m_menubar_item_help;

	ATTRIBUTE_FIELD_WIDGET_NAME(RigEditorMenubarLayout, m_menubar_item_wheels, "menubar_item_wheels");
	MyGUI::MenuItem* m_menubar_item_wheels;

	ATTRIBUTE_FIELD_WIDGET_NAME(RigEditorMenubarLayout, m_wheels_popup, "wheels_popup");
	MyGUI::PopupMenu* m_wheels_popup;

	ATTRIBUTE_FIELD_WIDGET_NAME(RigEditorMenubarLayout, m_wheels_popup_item_select_all, "wheels_popup_item_select_all");
	MyGUI::MenuItem* m_wheels_popup_item_select_all;

	ATTRIBUTE_FIELD_WIDGET_NAME(RigEditorMenubarLayout, m_wheels_popup_item_deselect_all, "wheels_popup_item_deselect_all");
	MyGUI::MenuItem* m_wheels_popup_item_deselect_all;

	//%LE Widget_Declaration list end
};

} // namespace GUI

} // namespace RoR

