#pragma once

#include "../ecs/entity/entity.h"

// --- Scene Tree --- //
// Maintains parent child relationship between nodes
typedef struct SceneTreeNode {
    Entity entity;
    struct SceneTreeNode* parent;
    struct SceneTreeNode** children;
    size_t childCount;
} SceneTreeNode;

SceneTreeNode* rbe_scene_tree_create_tree_node(Entity entity, SceneTreeNode* parent);

// --- Scene Manager --- //
void rbe_scene_manager_initialize();
void rbe_scene_manager_finalize();
void rbe_scene_manager_queue_entity_for_creation(Entity entity);
void rbe_scene_manager_process_queued_creation_entities();
void rbe_scene_manager_queue_entity_for_deletion(Entity entity);
void rbe_scene_manager_process_queued_deletion_entities();
void rbe_scene_manager_queue_scene_change(const char* scenePath);
void rbe_scene_manager_process_queued_scene_change();
void rbe_scene_manager_set_active_scene_root(SceneTreeNode* root);
