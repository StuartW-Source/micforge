// lilv (LV2 hosting library), loaded at run time. Include ONLY from lv2host.cpp, after <lilv/lilv.h>.
// If liblilv isn't installed, MicForge still starts and LV2 plugins are reported as unavailable.
// Calls in lv2host.cpp keep their normal lilv names; the #defines below route them through
// function pointers filled in by mf::lilvapi::load(). (lilv_instance_run/activate/deactivate/
// connect_port are static inline in lilv.h and need no loading.)
#pragma once

#include <lilv/lilv.h>

#include <string>

#define MF_LILV_FUNCTIONS(X) \
    X(lilv_instance_free) \
    X(lilv_new_uri) \
    X(lilv_node_as_float) \
    X(lilv_node_as_string) \
    X(lilv_node_as_uri) \
    X(lilv_node_free) \
    X(lilv_nodes_free) \
    X(lilv_nodes_get) \
    X(lilv_nodes_get_first) \
    X(lilv_nodes_size) \
    X(lilv_nodes_begin) \
    X(lilv_nodes_is_end) \
    X(lilv_nodes_next) \
    X(lilv_plugin_class_get_label) \
    X(lilv_plugin_get_author_name) \
    X(lilv_plugin_get_class) \
    X(lilv_plugin_get_name) \
    X(lilv_plugin_get_num_ports) \
    X(lilv_plugin_get_num_ports_of_class) \
    X(lilv_plugin_get_port_by_index) \
    X(lilv_plugin_get_port_ranges_float) \
    X(lilv_plugin_get_required_features) \
    X(lilv_plugin_get_uri) \
    X(lilv_plugin_instantiate) \
    X(lilv_plugins_get) \
    X(lilv_plugins_get_by_uri) \
    X(lilv_plugins_begin) \
    X(lilv_plugins_is_end) \
    X(lilv_plugins_next) \
    X(lilv_port_get_name) \
    X(lilv_port_get_scale_points) \
    X(lilv_port_get_symbol) \
    X(lilv_port_get_value) \
    X(lilv_port_has_property) \
    X(lilv_port_is_a) \
    X(lilv_scale_point_get_label) \
    X(lilv_scale_point_get_value) \
    X(lilv_scale_points_free) \
    X(lilv_scale_points_get) \
    X(lilv_scale_points_begin) \
    X(lilv_scale_points_is_end) \
    X(lilv_scale_points_next) \
    X(lilv_world_find_nodes) \
    X(lilv_world_get_all_plugins) \
    X(lilv_world_load_all) \
    X(lilv_world_new)

namespace mf::lilvapi {
#define MF_LILV_DECLARE(fn) extern decltype(&::fn) fn;
MF_LILV_FUNCTIONS(MF_LILV_DECLARE)
#undef MF_LILV_DECLARE
bool load(std::string *error); // thread-safe, loads once; false + error if unavailable
} // namespace mf::lilvapi

#ifndef MF_LILV_NO_REDIRECT
#define lilv_instance_free ::mf::lilvapi::lilv_instance_free
#define lilv_new_uri ::mf::lilvapi::lilv_new_uri
#define lilv_node_as_float ::mf::lilvapi::lilv_node_as_float
#define lilv_node_as_string ::mf::lilvapi::lilv_node_as_string
#define lilv_node_as_uri ::mf::lilvapi::lilv_node_as_uri
#define lilv_node_free ::mf::lilvapi::lilv_node_free
#define lilv_nodes_free ::mf::lilvapi::lilv_nodes_free
#define lilv_nodes_get ::mf::lilvapi::lilv_nodes_get
#define lilv_nodes_get_first ::mf::lilvapi::lilv_nodes_get_first
#define lilv_nodes_size ::mf::lilvapi::lilv_nodes_size
#define lilv_nodes_begin ::mf::lilvapi::lilv_nodes_begin
#define lilv_nodes_is_end ::mf::lilvapi::lilv_nodes_is_end
#define lilv_nodes_next ::mf::lilvapi::lilv_nodes_next
#define lilv_plugin_class_get_label ::mf::lilvapi::lilv_plugin_class_get_label
#define lilv_plugin_get_author_name ::mf::lilvapi::lilv_plugin_get_author_name
#define lilv_plugin_get_class ::mf::lilvapi::lilv_plugin_get_class
#define lilv_plugin_get_name ::mf::lilvapi::lilv_plugin_get_name
#define lilv_plugin_get_num_ports ::mf::lilvapi::lilv_plugin_get_num_ports
#define lilv_plugin_get_num_ports_of_class ::mf::lilvapi::lilv_plugin_get_num_ports_of_class
#define lilv_plugin_get_port_by_index ::mf::lilvapi::lilv_plugin_get_port_by_index
#define lilv_plugin_get_port_ranges_float ::mf::lilvapi::lilv_plugin_get_port_ranges_float
#define lilv_plugin_get_required_features ::mf::lilvapi::lilv_plugin_get_required_features
#define lilv_plugin_get_uri ::mf::lilvapi::lilv_plugin_get_uri
#define lilv_plugin_instantiate ::mf::lilvapi::lilv_plugin_instantiate
#define lilv_plugins_get ::mf::lilvapi::lilv_plugins_get
#define lilv_plugins_get_by_uri ::mf::lilvapi::lilv_plugins_get_by_uri
#define lilv_plugins_begin ::mf::lilvapi::lilv_plugins_begin
#define lilv_plugins_is_end ::mf::lilvapi::lilv_plugins_is_end
#define lilv_plugins_next ::mf::lilvapi::lilv_plugins_next
#define lilv_port_get_name ::mf::lilvapi::lilv_port_get_name
#define lilv_port_get_scale_points ::mf::lilvapi::lilv_port_get_scale_points
#define lilv_port_get_symbol ::mf::lilvapi::lilv_port_get_symbol
#define lilv_port_get_value ::mf::lilvapi::lilv_port_get_value
#define lilv_port_has_property ::mf::lilvapi::lilv_port_has_property
#define lilv_port_is_a ::mf::lilvapi::lilv_port_is_a
#define lilv_scale_point_get_label ::mf::lilvapi::lilv_scale_point_get_label
#define lilv_scale_point_get_value ::mf::lilvapi::lilv_scale_point_get_value
#define lilv_scale_points_free ::mf::lilvapi::lilv_scale_points_free
#define lilv_scale_points_get ::mf::lilvapi::lilv_scale_points_get
#define lilv_scale_points_begin ::mf::lilvapi::lilv_scale_points_begin
#define lilv_scale_points_is_end ::mf::lilvapi::lilv_scale_points_is_end
#define lilv_scale_points_next ::mf::lilvapi::lilv_scale_points_next
#define lilv_world_find_nodes ::mf::lilvapi::lilv_world_find_nodes
#define lilv_world_get_all_plugins ::mf::lilvapi::lilv_world_get_all_plugins
#define lilv_world_load_all ::mf::lilvapi::lilv_world_load_all
#define lilv_world_new ::mf::lilvapi::lilv_world_new
#endif
