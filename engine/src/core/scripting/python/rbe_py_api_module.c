#include "rbe_py_api_module.h"

#include <string.h>

#include "py_cache.h"
#include "py_script_context.h"
#include "../../game_properties.h"
#include "../../engine_context.h"
#include "../../asset_manager.h"
#include "../../input/input.h"
#include "../../audio/audio_manager.h"
#include "../../scripting/script_context.h"
#include "../../scripting/python/py_helper.h"
#include "../../scene/scene_manager.h"
#include "../../physics/collision/collision.h"
#include "../../camera/camera.h"
#include "../../camera/camera_manager.h"
#include "../../ecs/ecs_manager.h"
#include "../../ecs/system/ec_system.h"
#include "../../ecs/component/animated_sprite_component.h"
#include "../../ecs/component/collider2d_component.h"
#include "../../ecs/component/color_square_component.h"
#include "../../ecs/component/node_component.h"
#include "../../ecs/component/script_component.h"
#include "../../ecs/component/sprite_component.h"
#include "../../ecs/component/text_label_component.h"
#include "../../networking/rbe_network.h"
#include "../../utils/rbe_string_util.h"
#include "../../utils/rbe_assert.h"

#ifdef _MSC_VER
#pragma warning(disable : 4996) // for strcpy
#endif

// TODO: Clean up strdups

//--- Node Utils ---//
void setup_scene_stage_nodes(SceneTreeNode* parent, PyObject* stageNodeList);
void setup_scene_component_node(Entity entity, PyObject* component);

//--- Py Utils ---//
PyObject* rbe_py_utils_get_entity_instance(Entity entity);

//--- RBE PY API ---//

// Engine
PyObject* rbe_py_api_engine_exit(PyObject* self, PyObject* args, PyObject* kwargs) {
    int exitCode;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", rbePyApiEngineExitKWList, &exitCode)) {
        RBEEngineContext* engineContext = rbe_engine_context_get();
        engineContext->isRunning = false;
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_engine_set_target_fps(PyObject* self, PyObject* args, PyObject* kwargs) {
    int targetFPS;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", rbePyApiEngineSetTargetFPSKWList, &targetFPS)) {
        RBEEngineContext* engineContext = rbe_engine_context_get();
        engineContext->targetFPS = targetFPS;
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_engine_get_target_fps(PyObject* self, PyObject* args) {
    const RBEEngineContext* engineContext = rbe_engine_context_get();
    return Py_BuildValue("(f)", engineContext->targetFPS);
}

PyObject* rbe_py_api_engine_get_average_fps(PyObject* self, PyObject* args) {
    const RBEEngineContext* engineContext = rbe_engine_context_get();
    return Py_BuildValue("(f)", engineContext->averageFPS);
}

PyObject* rbe_py_api_engine_set_fps_display_enabled(PyObject* self, PyObject* args, PyObject* kwargs) {
    bool isEnabled;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "b", rbePyApiGenericEnabledKWList, &isEnabled)) {
        rbe_ecs_manager_enable_fps_display_entity(isEnabled);
        Py_RETURN_NONE;
    }
    return NULL;
}

// Configure
PyObject* rbe_py_api_configure_game(PyObject* self, PyObject* args, PyObject* kwargs) {
    char* gameTitle;
    int windowWidth;
    int windowHeight;
    int resolutionWidth;
    int resolutionHeight;
    int targetFPS;
    char* initialScenePath;
    bool collidersVisible = false;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "siiiiisb", rbePyApiProjectConfigureKWList, &gameTitle, &windowWidth, &windowHeight, &resolutionWidth, &resolutionHeight, &targetFPS, &initialScenePath, &collidersVisible)) {
        RBEGameProperties* gameProperties = rbe_game_props_get();
        gameProperties->gameTitle = rbe_strdup(gameTitle);
        gameProperties->windowWidth = windowWidth;
        gameProperties->windowHeight = windowHeight;
        gameProperties->resolutionWidth = resolutionWidth;
        gameProperties->resolutionHeight = resolutionHeight;
        gameProperties->targetFPS = targetFPS;
        gameProperties->initialScenePath = rbe_strdup(initialScenePath);
        gameProperties->areCollidersVisible = collidersVisible;
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_configure_assets(PyObject* self, PyObject* args, PyObject* kwargs) {
    PyObject* audioSourcesList;
    PyObject* texturesList;
    PyObject* fontsList;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "OOO", rbePyApiConfigureAssetsKWList, &audioSourcesList, &texturesList, &fontsList)) {
        RBE_ASSERT_FMT(PyList_Check(audioSourcesList), "Passed in audio source assets are not a python list, check python api implementation...");
        RBE_ASSERT_FMT(PyList_Check(texturesList), "Passed in texture assets are not a python list, check python api implementation...");
        RBE_ASSERT_FMT(PyList_Check(fontsList), "Passed in font assets are not a python list, check python api implementation...");

        RBEGameProperties* gameProperties = rbe_game_props_get();

        // Audio Sources
        rbe_logger_debug("audio_sources:");
        for (Py_ssize_t i = 0; i < PyList_Size(audioSourcesList); i++) {
            PyObject* pAudioSourceAsset = PyList_GetItem(audioSourcesList, i);
            RBE_ASSERT(pAudioSourceAsset != NULL);
            const char* filePath = phy_get_string_from_var(pAudioSourceAsset, "file_path");
            rbe_logger_debug("file_path = %s", filePath);
            RBEAssetAudioSource assetAudioSource = { .file_path = rbe_strdup(filePath) };
            gameProperties->audioSources[gameProperties->audioSourceCount++] = assetAudioSource;
        }

        // Textures
        rbe_logger_debug("textures:");
        for (Py_ssize_t i = 0; i < PyList_Size(texturesList); i++) {
            PyObject* pTextureAsset = PyList_GetItem(texturesList, i);
            RBE_ASSERT(pTextureAsset != NULL);
            const char* filePath = phy_get_string_from_var(pTextureAsset, "file_path");
            const char* wrapS = phy_get_string_from_var(pTextureAsset, "wrap_s");
            const char* wrapT = phy_get_string_from_var(pTextureAsset, "wrap_t");
            const char* filterMin = phy_get_string_from_var(pTextureAsset, "filter_min");
            const char* filterMag = phy_get_string_from_var(pTextureAsset, "filter_mag");
            rbe_logger_debug("file_path = %s, wrap_s = %s, wrap_t = %s, filter_min = %s, filter_mag = %s",
                             filePath, wrapS, wrapT, filterMin, filterMag);
            RBEAssetTexture assetTexture = { .file_path = rbe_strdup(filePath) };
            gameProperties->textures[gameProperties->textureCount++] = assetTexture;
        }

        // Fonts
        rbe_logger_debug("fonts:");
        for (Py_ssize_t i = 0; i < PyList_Size(fontsList); i++) {
            PyObject* pFontAsset = PyList_GetItem(fontsList, i);
            RBE_ASSERT(pFontAsset != NULL);
            const char* filePath = phy_get_string_from_var(pFontAsset, "file_path");
            const char* uid = phy_get_string_from_var(pFontAsset, "uid");
            const int size = phy_get_int_from_var(pFontAsset, "size");
            rbe_logger_debug("file_path = %s, uid = %s, size = %d", filePath, uid, size);
            RBEAssetFont assetFont = { .file_path = rbe_strdup(filePath), .uid = rbe_strdup(uid), .size = size };
            gameProperties->fonts[gameProperties->fontCount++] = assetFont;
        }

        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_configure_inputs(PyObject* self, PyObject* args, PyObject* kwargs) {
    PyObject* inputActionsList;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "O", rbePyApiConfigureInputsKWList, &inputActionsList)) {
        RBE_ASSERT_FMT(PyList_Check(inputActionsList), "Passed in input actions are not a python list, check python api implementation...");

        RBEGameProperties* gameProperties = rbe_game_props_get();

        rbe_logger_debug("input actions:");
        for (Py_ssize_t i = 0; i < PyList_Size(inputActionsList); i++) {
            PyObject* pInputAction = PyList_GetItem(inputActionsList, i);
            RBE_ASSERT(pInputAction != NULL);
            const char* actionName = phy_get_string_from_var(pInputAction, "name");
            const int actionDeviceId = phy_get_int_from_var(pInputAction, "device_id");
            rbe_logger_debug("name = '%s', device_id = '%d'", actionName, actionDeviceId);
            PyObject* valuesList = PyObject_GetAttrString(pInputAction, "values");
            RBE_ASSERT(valuesList != NULL);
            RBE_ASSERT_FMT(PyList_Check(valuesList), "Input action values for '%s' is not a list!  Check python api implementation.", actionName);
            Py_ssize_t valueListSize = PyList_Size(valuesList);
            RBEInputAction inputAction = { .name = rbe_strdup(actionName), .deviceId = actionDeviceId, .valueCount = (size_t) valueListSize };
            for (Py_ssize_t actionIndex = 0; actionIndex < valueListSize; actionIndex++) {
                PyObject* pActionValue = PyList_GetItem(valuesList, actionIndex);
                const char* actionValue = pyh_get_string_from_obj(pActionValue);
                rbe_logger_debug("action value = '%s'", actionValue);
                inputAction.values[actionIndex] = rbe_strdup(actionValue);
            }
            gameProperties->inputActions[gameProperties->inputActionCount++] = inputAction;
        }
        Py_RETURN_NONE;
    }
    return NULL;
}

// Stage
PyObject* rbe_py_api_create_stage_nodes(PyObject* self, PyObject* args, PyObject* kwargs) {
    PyObject* stageNodeList;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "O", rbePyApiCreateStageNodesKWList, &stageNodeList)) {
        RBE_ASSERT_FMT(PyList_Check(stageNodeList), "Passed in stage nodes are not a python list, check python api implementation...");
        rbe_logger_debug("setup stage nodes:");
        setup_scene_stage_nodes(NULL_ENTITY, stageNodeList); // Assumes this is the root entity node for the scene
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* PyInit_rbe_py_API(void) {
    return PyModule_Create(&rbePyAPIModDef);
}

//--- Node Utils ---//
void setup_scene_stage_nodes(SceneTreeNode* parent, PyObject* stageNodeList) {
    for (Py_ssize_t i = 0; i < PyList_Size(stageNodeList); i++) {
        SceneTreeNode* node = rbe_scene_tree_create_tree_node(rbe_ec_system_create_entity(), parent);
        // Set tree root if parent is absent
        if (parent == NULL) {
            rbe_scene_manager_set_active_scene_root(node);
        } else {
            parent->children[parent->childCount++] = node;
        }

        PyObject* pStageNode = PyList_GetItem(stageNodeList, i);
        // Node component is used for all scene nodes
        const char* nodeName = phy_get_string_from_var(pStageNode, "name");
        const char* nodeType = phy_get_string_from_var(pStageNode, "type");
        NodeComponent* nodeComponent = node_component_create();
        strcpy(nodeComponent->name, nodeName);
        nodeComponent->type = node_get_base_type(nodeType);
        RBE_ASSERT_FMT(nodeComponent->type != NodeBaseType_INVALID, "Node '%s' has an invalid node type '%s'", nodeName, nodeType);
        component_manager_set_component(node->entity, ComponentDataIndex_NODE, nodeComponent);

        // Process tags if tags var is a list
        PyObject* tagsListVar = PyObject_GetAttrString(pStageNode, "tags");
        if (PyList_Check(tagsListVar)) {
            for (Py_ssize_t tagIndex = 0; tagIndex < PyList_Size(tagsListVar); tagIndex++) {}
        }
        // TODO: Actually load external scene file here and updated variables
        PyObject* externalNodeSourceVar = PyObject_GetAttrString(pStageNode, "external_node_source");
        if (externalNodeSourceVar != Py_None) {
            const char* externalNodeSourcePath = phy_get_string_from_var(externalNodeSourceVar, "external_node_source");
        }
        // Components
        // Testing module getting stuff to store component classes
        PyObject* componentsListVar = PyObject_GetAttrString(pStageNode, "components");
        if (PyList_Check(componentsListVar)) {
            for (Py_ssize_t componentIndex = 0; componentIndex < PyList_Size(componentsListVar); componentIndex++) {
                PyObject* pComponent = PyList_GetItem(componentsListVar, componentIndex);
                RBE_ASSERT(pComponent != NULL);
                setup_scene_component_node(node->entity, pComponent);
            }
        }
        // TODO: Do in a different step or having different functionality to add node to scene tree
        rbe_ec_system_update_entity_signature_with_systems(node->entity);
        // Children Nodes
        PyObject* childrenListVar = PyObject_GetAttrString(pStageNode, "children");
        if (PyList_Check(childrenListVar)) {
            // Recurse through children nodes
            setup_scene_stage_nodes(node, childrenListVar);
        }

        rbe_scene_manager_queue_entity_for_creation(node); // May move in a different place TODO: Figure out...

        rbe_logger_debug("node_name = %s, node_type = %s", nodeName, nodeType);
        Py_DECREF(externalNodeSourceVar);
    }
}

void setup_scene_component_node(Entity entity, PyObject* component) {
    const char* className = Py_TYPE(component)->tp_name; // TODO: Should probably Py_DecRed()?
    if (strcmp(className, "Transform2DComponent") == 0) {
        rbe_logger_debug("Building transform 2d component");
        PyObject* pPosition = PyObject_GetAttrString(component, "position");
        const float positionX = phy_get_float_from_var(pPosition, "x");
        const float positionY = phy_get_float_from_var(pPosition, "y");
        PyObject* pScale = PyObject_GetAttrString(component, "scale");
        const float scaleX = phy_get_float_from_var(pScale, "x");
        const float scaleY = phy_get_float_from_var(pScale, "y");
        const float rotation = phy_get_float_from_var(component, "rotation");
        const int zIndex = phy_get_int_from_var(component, "z_index");
        const bool zIndexRelativeToParent = phy_get_bool_from_var(component, "z_index_relative_to_parent");
        const bool ignoreCamera = phy_get_bool_from_var(component, "ignore_camera");
        Transform2DComponent* transform2DComponent = transform2d_component_create();
        transform2DComponent->localTransform.position.x = positionX;
        transform2DComponent->localTransform.position.y = positionY;
        transform2DComponent->localTransform.scale.x = scaleX;
        transform2DComponent->localTransform.scale.y = scaleY;
        transform2DComponent->localTransform.rotation = rotation;
        transform2DComponent->zIndex = zIndex;
        transform2DComponent->isZIndexRelativeToParent = zIndexRelativeToParent;
        transform2DComponent->ignoreCamera = ignoreCamera;
        component_manager_set_component(entity, ComponentDataIndex_TRANSFORM_2D, transform2DComponent);
        rbe_logger_debug("position: (%f, %f), scale: (%f, %f), rotation: %f, z_index: %d, z_index_relative: %d, ignore_camera: %d",
                         positionX, positionY, scaleX, scaleY, rotation, zIndex, zIndexRelativeToParent, ignoreCamera);
        Py_DECREF(pPosition);
        Py_DECREF(pScale);
    } else if (strcmp(className, "SpriteComponent") == 0) {
        rbe_logger_debug("Building sprite component");
        const char* texturePath = phy_get_string_from_var(component, "texture_path");
        PyObject* pDrawSource = PyObject_GetAttrString(component, "draw_source");
        const float drawSourceX = phy_get_float_from_var(pDrawSource, "x");
        const float drawSourceY = phy_get_float_from_var(pDrawSource, "y");
        const float drawSourceW = phy_get_float_from_var(pDrawSource, "w");
        const float drawSourceH = phy_get_float_from_var(pDrawSource, "h");
        PyObject* pOrigin = PyObject_GetAttrString(component, "origin");
        const float originX = phy_get_float_from_var(pOrigin, "x");
        const float originY = phy_get_float_from_var(pOrigin, "y");
        const bool flipX = phy_get_bool_from_var(component, "flip_x");
        const bool flipY = phy_get_bool_from_var(component, "flip_y");
        PyObject* pModulate = PyObject_GetAttrString(component, "modulate");
        const int modulateR = phy_get_int_from_var(pModulate, "r");
        const int modulateG = phy_get_int_from_var(pModulate, "g");
        const int modulateB = phy_get_int_from_var(pModulate, "b");
        const int modulateA = phy_get_int_from_var(pModulate, "a");
        SpriteComponent* spriteComponent = sprite_component_create();
        spriteComponent->texture = rbe_asset_manager_get_texture(texturePath);
        RBE_ASSERT_FMT(spriteComponent->texture != NULL, "Unable to read texture path '%s'", texturePath);
        spriteComponent->drawSource.x = drawSourceX;
        spriteComponent->drawSource.y = drawSourceY;
        spriteComponent->drawSource.w = drawSourceW;
        spriteComponent->drawSource.h = drawSourceH;
        spriteComponent->origin.x = originX;
        spriteComponent->origin.y = originY;
        spriteComponent->flipX = flipX;
        spriteComponent->flipY = flipY;
        const Color modulateColor = rbe_color_get_normalized_color(modulateR, modulateG, modulateB, modulateA);
        spriteComponent->modulate.r = modulateColor.r;
        spriteComponent->modulate.g = modulateColor.g;
        spriteComponent->modulate.b = modulateColor.b;
        spriteComponent->modulate.a = modulateColor.a;
        component_manager_set_component(entity, ComponentDataIndex_SPRITE, spriteComponent);
        rbe_logger_debug("texture_path = %s, draw_source = (%f, %f, %f, %f), origin: (%f, %f), flip_x: %d, flip_y: %d, modulate: (%d, %d, %d, %d)",
                         texturePath, drawSourceX, drawSourceY, drawSourceW, drawSourceH, originX, originY, flipX, flipY, modulateR, modulateG, modulateB, modulateA);
        Py_DECREF(pDrawSource);
        Py_DECREF(pOrigin);
        Py_DECREF(pModulate);
    } else if (strcmp(className, "AnimatedSpriteComponent") == 0) {
        rbe_logger_debug("Building animated sprite component");
        AnimatedSpriteComponent* animatedSpriteComponent = animated_sprite_component_create();
        const char* currentAnimationName = phy_get_string_from_var(component, "current_animation_name");
        const bool isPlaying = phy_get_bool_from_var(component, "is_playing");
        PyObject* pOrigin = PyObject_GetAttrString(component, "origin");
        const float originX = phy_get_float_from_var(pOrigin, "x");
        const float originY = phy_get_float_from_var(pOrigin, "y");
        const bool flipX = phy_get_bool_from_var(component, "flip_x");
        const bool flipY = phy_get_bool_from_var(component, "flip_y");
        rbe_logger_debug("current_animation_name: '%s', is_playing: '%d', origin: (%f, %f), flip_x: '%d', flip_y: '%d'",
                         currentAnimationName, isPlaying, originX, originY, flipX, flipY);
        animatedSpriteComponent->isPlaying = isPlaying;
        animatedSpriteComponent->origin.x = originX;
        animatedSpriteComponent->origin.y = originY;
        animatedSpriteComponent->flipX = flipX;
        animatedSpriteComponent->flipY = flipY;

        PyObject* pyAnimationsList = PyObject_GetAttrString(component, "animations");
        RBE_ASSERT(PyList_Check(pyAnimationsList));
        for (Py_ssize_t animationIndex = 0; animationIndex < PyList_Size(pyAnimationsList); animationIndex++) {
            Animation animation;
            animation.frameCount = 0;
            animation.currentFrame = 0;
            animation.isValid = true;
            PyObject* pyAnimation = PyList_GetItem(pyAnimationsList, animationIndex);
            RBE_ASSERT(pyAnimation != NULL);
            const char* animationName = phy_get_string_from_var(pyAnimation, "name");
            const int animationSpeed = phy_get_int_from_var(pyAnimation, "speed");
            const bool animationLoops = phy_get_bool_from_var(pyAnimation, "loops");
            rbe_logger_debug("building anim - name: '%s', speed: '%d', loops: '%d'", animationName, animationSpeed, animationLoops);
            strcpy(animation.name, animationName);
            animation.speed = animationSpeed;
            animation.doesLoop = animationLoops;

            PyObject* pyAnimationFramesList = PyObject_GetAttrString(pyAnimation, "frames");
            RBE_ASSERT(PyList_Check(pyAnimationFramesList));
            for (Py_ssize_t animationFrameIndex = 0; animationFrameIndex < PyList_Size(pyAnimationFramesList); animationFrameIndex++) {
                PyObject* pyAnimationFrame = PyList_GetItem(pyAnimationFramesList, animationFrameIndex);
                const int animationFrameNumber = phy_get_int_from_var(pyAnimationFrame, "frame");
                const char* animationFrameTexturePath = phy_get_string_from_var(pyAnimationFrame, "texture_path");
                PyObject* pyDrawSource = PyObject_GetAttrString(pyAnimationFrame, "draw_source");
                RBE_ASSERT(pyDrawSource != NULL);
                const float drawSourceX = phy_get_float_from_var(pyDrawSource, "x");
                const float drawSourceY = phy_get_float_from_var(pyDrawSource, "y");
                const float drawSourceW = phy_get_float_from_var(pyDrawSource, "w");
                const float drawSourceH = phy_get_float_from_var(pyDrawSource, "h");
                rbe_logger_debug("frame: %d, texture_path: %s, draw_source: (%f, %f, %f, %f)",
                                 animationFrameNumber, animationFrameTexturePath, drawSourceX, drawSourceY, drawSourceW, drawSourceH);
                AnimationFrame animationFrame;
                animationFrame.texture = rbe_asset_manager_get_texture(animationFrameTexturePath);
                RBE_ASSERT(animationFrame.texture != NULL);
                animationFrame.frame = animationFrameNumber;
                const Rect2 frameDrawSource = { drawSourceX, drawSourceY, drawSourceW, drawSourceH };
                animationFrame.drawSource = frameDrawSource;
                animation.animationFrames[animationFrame.frame] = animationFrame;
                animation.frameCount++;

                Py_DECREF(pyDrawSource);
            }

            // Set current animation if the name matches
            animated_sprite_component_add_animation(animatedSpriteComponent, animation);
            if (strcmp(animation.name, currentAnimationName) == 0) {
                animatedSpriteComponent->currentAnimation = animation;
            }
        }

        component_manager_set_component(entity, ComponentDataIndex_ANIMATED_SPRITE, animatedSpriteComponent);

        Py_DECREF(pOrigin);
    } else if (strcmp(className, "TextLabelComponent") == 0) {
        rbe_logger_debug("Building text label component");
        const char* textLabelUID = phy_get_string_from_var(component, "uid");
        const char* textLabelText = phy_get_string_from_var(component, "text");
        PyObject* pColor = PyObject_GetAttrString(component, "color");
        const int colorR = phy_get_int_from_var(pColor, "r");
        const int colorG = phy_get_int_from_var(pColor, "g");
        const int colorB = phy_get_int_from_var(pColor, "b");
        const int colorA = phy_get_int_from_var(pColor, "a");
        const Color textLabelColor = rbe_color_get_normalized_color(colorR, colorG, colorB, colorA);
        TextLabelComponent* textLabelComponent = text_label_component_create();
        textLabelComponent->font = rbe_asset_manager_get_font(textLabelUID);
        RBE_ASSERT(textLabelComponent->font != NULL);
        strcpy(textLabelComponent->text, textLabelText);
        textLabelComponent->color = textLabelColor;
        component_manager_set_component(entity, ComponentDataIndex_TEXT_LABEL, textLabelComponent);
        rbe_logger_debug("uid: %s, text: %s, color(%d, %d, %d, %d)", textLabelUID, textLabelText, colorR, colorG, colorB, colorA);
        Py_DECREF(pColor);
    } else if (strcmp(className, "ScriptComponent") == 0) {
        rbe_logger_debug("Building script component");
        const char* scriptClassPath = phy_get_string_from_var(component, "class_path");
        const char* scriptClassName = phy_get_string_from_var(component, "class_name");
        ScriptComponent* scriptComponent = script_component_create();
        scriptComponent->classPath = scriptClassPath;
        scriptComponent->className = scriptClassName;
        scriptComponent->contextType = ScriptContextType_PYTHON;
        component_manager_set_component(entity, ComponentDataIndex_SCRIPT, scriptComponent);
        rbe_logger_debug("class_path: %s, class_name: %s", scriptClassPath, scriptClassName);
    } else if (strcmp(className, "Collider2DComponent") == 0) {
        rbe_logger_debug("Building collider2d component");
        PyObject* pyExtents = PyObject_GetAttrString(component, "extents");
        RBE_ASSERT(pyExtents != NULL);
        const float rectW = phy_get_float_from_var(pyExtents, "w");
        const float rectH = phy_get_float_from_var(pyExtents, "h");
        PyObject* pyColor = PyObject_GetAttrString(component, "color");
        RBE_ASSERT(pyColor != NULL);
        const int colorR = phy_get_int_from_var(pyColor, "r");
        const int colorG = phy_get_int_from_var(pyColor, "g");
        const int colorB = phy_get_int_from_var(pyColor, "b");
        const int colorA = phy_get_int_from_var(pyColor, "a");
        Collider2DComponent* collider2DComponent = collider2d_component_create();
        collider2DComponent->extents.w = rectW;
        collider2DComponent->extents.h = rectH;
        collider2DComponent->color.r = (float) colorR / 255.0f;
        collider2DComponent->color.g = (float) colorG / 255.0f;
        collider2DComponent->color.b = (float) colorB / 255.0f;
        collider2DComponent->color.a = (float) colorA / 255.0f;
        collider2DComponent->collisionExceptionCount = 0;
        component_manager_set_component(entity, ComponentDataIndex_COLLIDER_2D, collider2DComponent);
        rbe_logger_debug("extents: (%f, %f), color: (%f, %f, %f, %f)",
                         rectW, rectH, collider2DComponent->color.r, collider2DComponent->color.g, collider2DComponent->color.b, collider2DComponent->color.a);

        Py_DECREF(pyExtents);
        Py_DECREF(pyColor);
    } else if (strcmp(className, "ColorSquareComponent") == 0) {
        rbe_logger_debug("Building collider2d component");
        PyObject* pySize = PyObject_GetAttrString(component, "size");
        RBE_ASSERT(pySize != NULL);
        const float rectW = phy_get_float_from_var(pySize, "w");
        const float rectH = phy_get_float_from_var(pySize, "h");
        PyObject* pyColor = PyObject_GetAttrString(component, "color");
        RBE_ASSERT(pyColor != NULL);
        const int colorR = phy_get_int_from_var(pyColor, "r");
        const int colorG = phy_get_int_from_var(pyColor, "g");
        const int colorB = phy_get_int_from_var(pyColor, "b");
        const int colorA = phy_get_int_from_var(pyColor, "a");
        ColorSquareComponent* colorSquareComponent = color_square_component_create();
        colorSquareComponent->size.w = rectW;
        colorSquareComponent->size.h = rectH;
        colorSquareComponent->color.r = (float) colorR / 255.0f;
        colorSquareComponent->color.g = (float) colorG / 255.0f;
        colorSquareComponent->color.b = (float) colorB / 255.0f;
        colorSquareComponent->color.a = (float) colorA / 255.0f;
        component_manager_set_component(entity, ComponentDataIndex_COLOR_SQUARE, colorSquareComponent);
        rbe_logger_debug("size: (%f, %f), color: (%f, %f, %f, %f)",
                         rectW, rectH, colorSquareComponent->color.r, colorSquareComponent->color.g,
                         colorSquareComponent->color.b, colorSquareComponent->color.a);

        Py_DECREF(pySize);
        Py_DECREF(pyColor);
    } else {
        rbe_logger_error("Invalid component class name: '%s'", className);
    }
}

// Input
PyObject* rbe_py_api_input_add_action(PyObject* self, PyObject* args, PyObject* kwargs) {
    char* actionName;
    char* actionValue;
    int deviceId;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "ssi", rbePyApiInputAddActionKWList, &actionName, &actionValue, &deviceId)) {
        rbe_input_add_action_value(actionName, actionValue, deviceId);
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_input_is_action_pressed(PyObject* self, PyObject* args, PyObject* kwargs) {
    char* actionName;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "s", rbePyApiInputActionInputCheckKWList, &actionName)) {
        if (rbe_input_is_action_pressed(actionName)) {
            Py_RETURN_TRUE;
        }
        Py_RETURN_FALSE;
    }
    return NULL;
}

PyObject* rbe_py_api_input_is_action_just_pressed(PyObject* self, PyObject* args, PyObject* kwargs) {
    char* actionName;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "s", rbePyApiInputActionInputCheckKWList, &actionName)) {
        if (rbe_input_is_action_just_pressed(actionName)) {
            Py_RETURN_TRUE;
        }
        Py_RETURN_FALSE;
    }
    return NULL;
}

PyObject* rbe_py_api_input_is_action_just_released(PyObject* self, PyObject* args, PyObject* kwargs) {
    char* actionName;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "s", rbePyApiInputActionInputCheckKWList, &actionName)) {
        if (rbe_input_is_action_just_released(actionName)) {
            Py_RETURN_TRUE;
        }
        Py_RETURN_FALSE;
    }
    return NULL;
}

// Camera
PyObject* rbe_py_api_camera2D_set_position(PyObject* self, PyObject* args, PyObject* kwargs) {
    float x;
    float y;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "ff", rbePyApiGenericXYKWList, &x, &y)) {
        RBECamera2D* camera2D = rbe_camera_manager_get_current_camera();
        camera2D->viewport.x = x;
        camera2D->viewport.y = y;
        rbe_camera2d_clamp_viewport_to_boundary(camera2D);
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_camera2D_add_to_position(PyObject* self, PyObject* args, PyObject* kwargs) {
    float x;
    float y;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "ff", rbePyApiGenericXYKWList, &x, &y)) {
        RBECamera2D* camera2D = rbe_camera_manager_get_current_camera();
        camera2D->viewport.x += x;
        camera2D->viewport.y += y;
        rbe_camera2d_clamp_viewport_to_boundary(camera2D);
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_camera2D_get_position(PyObject* self, PyObject* args) {
    RBECamera2D* camera2D = rbe_camera_manager_get_current_camera();
    return Py_BuildValue("(ff)", camera2D->viewport.x, camera2D->viewport.y);
}

PyObject* rbe_py_api_camera2D_set_offset(PyObject* self, PyObject* args, PyObject* kwargs) {
    float x;
    float y;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "ff", rbePyApiGenericXYKWList, &x, &y)) {
        RBECamera2D* camera2D = rbe_camera_manager_get_current_camera();
        camera2D->offset.x = x;
        camera2D->offset.y = y;
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_camera2D_add_to_offset(PyObject* self, PyObject* args, PyObject* kwargs) {
    float x;
    float y;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "ff", rbePyApiGenericXYKWList, &x, &y)) {
        RBECamera2D* camera2D = rbe_camera_manager_get_current_camera();
        camera2D->offset.x += x;
        camera2D->offset.y += y;
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_camera2D_get_offset(PyObject* self, PyObject* args) {
    RBECamera2D* camera2D = rbe_camera_manager_get_current_camera();
    return Py_BuildValue("(ff)", camera2D->offset.x, camera2D->offset.y);
}

PyObject* rbe_py_api_camera2D_set_zoom(PyObject* self, PyObject* args, PyObject* kwargs) {
    float x;
    float y;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "ff", rbePyApiGenericXYKWList, &x, &y)) {
        RBECamera2D* camera2D = rbe_camera_manager_get_current_camera();
        camera2D->zoom.x = x;
        camera2D->zoom.y = y;
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_camera2D_add_to_zoom(PyObject* self, PyObject* args, PyObject* kwargs) {
    float x;
    float y;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "ff", rbePyApiGenericXYKWList, &x, &y)) {
        RBECamera2D* camera2D = rbe_camera_manager_get_current_camera();
        camera2D->zoom.x += x;
        camera2D->zoom.y += y;
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_camera2D_get_zoom(PyObject* self, PyObject* args) {
    RBECamera2D* camera2D = rbe_camera_manager_get_current_camera();
    return Py_BuildValue("(ff)", camera2D->zoom.x, camera2D->zoom.y);
}

// Scene Tree
PyObject* rbe_py_api_scene_tree_change_scene(PyObject* self, PyObject* args, PyObject* kwargs) {
    char* scenePath;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "s", rbePyApiGenericPathKWList, &scenePath)) {
        rbe_scene_manager_queue_scene_change(scenePath);
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_camera2D_set_boundary(PyObject* self, PyObject* args, PyObject* kwargs) {
    float x;
    float y;
    float w;
    float h;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "ffff", rbePyApiGenericXYWHKWList, &x, &y, &w, &h)) {
        RBECamera2D* camera2D = rbe_camera_manager_get_current_camera();
        camera2D->boundary.x = x;
        camera2D->boundary.y = y;
        camera2D->boundary.w = w;
        camera2D->boundary.h = h;
        rbe_camera2d_clamp_viewport_to_boundary(camera2D);
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_camera2D_get_boundary(PyObject* self, PyObject* args) {
    RBECamera2D* camera2D = rbe_camera_manager_get_current_camera();
    return Py_BuildValue("(ffff)", camera2D->boundary.x, camera2D->boundary.y, camera2D->boundary.w, camera2D->boundary.h);
}

// Audio Manager
PyObject* rbe_py_api_audio_manager_play_sound(PyObject* self, PyObject* args, PyObject* kwargs) {
    char* audioPath;
    bool loops;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "sb", rbePyApiAudioManagerPlaySoundKWList, &audioPath, &loops)) {
        rbe_audio_manager_play_sound(audioPath, loops);
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_audio_manager_stop_sound(PyObject* self, PyObject* args, PyObject* kwargs) {
    char* audioPath;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "s", rbePyApiGenericPathKWList, &audioPath)) {
        rbe_audio_manager_stop_sound(audioPath);
        Py_RETURN_NONE;
    }
    return NULL;
}

// Node
PyObject* rbe_py_api_node_new(PyObject* self, PyObject* args, PyObject* kwargs) {
    char* classPath;
    char* className;
    char* nodeType;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "sss", rbePyApiNodeNewKWList, &classPath, &className, &nodeType)) {
        const Entity newEntity = rbe_ec_system_create_entity();

        // Setup script component first
        ScriptComponent* scriptComponent = script_component_create();
        scriptComponent->classPath = classPath;
        scriptComponent->className = className;
        scriptComponent->contextType = ScriptContextType_PYTHON;
        component_manager_set_component(newEntity, ComponentDataIndex_SCRIPT, scriptComponent);
        // Call create instance on script context
        // TODO: Not a big fan of updating the scripting system signature this way, but I guess it will suffice for now...
        rbe_ec_system_update_entity_signature_with_systems(newEntity);
        PyObject* entityInstance = rbe_py_get_script_instance(newEntity);
        RBE_ASSERT_FMT(entityInstance != NULL, "Entity instance '%d' is NULL!", newEntity);

        NodeComponent* nodeComponent = node_component_create();
        strcpy(nodeComponent->name, nodeType);
        nodeComponent->type = node_get_base_type(nodeType);
        RBE_ASSERT_FMT(nodeComponent->type != NodeBaseType_INVALID, "Node '%s' has an invalid node type '%s'", nodeType, nodeType);
        component_manager_set_component(newEntity, ComponentDataIndex_NODE, nodeComponent);

        const NodeBaseInheritanceType inheritanceType = node_get_type_inheritance(nodeComponent->type);

        if ((NodeBaseInheritanceType_NODE2D & inheritanceType) == NodeBaseInheritanceType_NODE2D) {
            Transform2DComponent* transform2DComponent = transform2d_component_create();
            component_manager_set_component(newEntity, ComponentDataIndex_TRANSFORM_2D, transform2DComponent);
        }

        if ((NodeBaseInheritanceType_SPRITE & inheritanceType) == NodeBaseInheritanceType_SPRITE) {
            SpriteComponent* spriteComponent = sprite_component_create();
            component_manager_set_component(newEntity, ComponentDataIndex_SPRITE, spriteComponent);
        }

        if ((NodeBaseInheritanceType_ANIMATED_SPRITE & inheritanceType) == NodeBaseInheritanceType_ANIMATED_SPRITE) {
            AnimatedSpriteComponent* animatedSpriteComponent = animated_sprite_component_create();
            component_manager_set_component(newEntity, ComponentDataIndex_ANIMATED_SPRITE, animatedSpriteComponent);
        }

        if ((NodeBaseInheritanceType_TEXT_LABEL & inheritanceType) == NodeBaseInheritanceType_TEXT_LABEL) {
            TextLabelComponent* textLabelComponent = text_label_component_create();
            component_manager_set_component(newEntity, ComponentDataIndex_TEXT_LABEL, textLabelComponent);
        }

        if ((NodeBaseInheritanceType_COLLIDER2D & inheritanceType) == NodeBaseInheritanceType_COLLIDER2D) {
            Collider2DComponent* collider2DComponent = collider2d_component_create();
            component_manager_set_component(newEntity, ComponentDataIndex_COLLIDER_2D, collider2DComponent);
        }

        if ((NodeBaseInheritanceType_COLOR_SQUARE & inheritanceType) == NodeBaseInheritanceType_COLOR_SQUARE) {
            ColorSquareComponent* colorSquareComponent = color_square_component_create();
            component_manager_set_component(newEntity, ComponentDataIndex_COLOR_SQUARE, colorSquareComponent);
        }

        Py_IncRef(entityInstance);
        return Py_BuildValue("O", entityInstance);
    }
    return NULL;
}

PyObject* rbe_py_api_node_queue_deletion(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity entity;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", rbePyApiGenericGetEntityKWList, &entity)) {
        SceneTreeNode* node = rbe_scene_manager_get_entity_tree_node(entity);
        rbe_queue_destroy_tree_node_entity_all(node);
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_node_add_child(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity parentEntity;
    Entity entity;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "ii", rbePyApiNodeAddChildKWList, &parentEntity, &entity)) {
        SceneTreeNode* parentNode = rbe_scene_manager_get_entity_tree_node(parentEntity);
        SceneTreeNode* node = rbe_scene_tree_create_tree_node(entity, parentNode);
        if (parentNode != NULL) {
            parentNode->children[parentNode->childCount++] = node;
        }

        rbe_ec_system_update_entity_signature_with_systems(entity);
        rbe_scene_manager_queue_entity_for_creation(node);
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_utils_get_entity_instance(Entity entity) {
#define TYPE_BUFFER_SIZE 32
    PyObject* scriptInstance = rbe_py_get_script_instance(entity);
    if (scriptInstance != NULL) {
        Py_IncRef(scriptInstance);
        return scriptInstance;
    }
    char typeBuffer[TYPE_BUFFER_SIZE];
    NodeComponent* nodeComponent = (NodeComponent*) component_manager_get_component(entity, ComponentDataIndex_NODE);
    strcpy(typeBuffer, node_get_component_type_string(nodeComponent->type));

    return Py_BuildValue("(is)", entity, typeBuffer);
#undef TYPE_BUFFER_SIZE
}

PyObject* rbe_py_api_node_get_child(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity parentEntity;
    char* childName;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "is", rbePyApiNodeGetChildKWList, &parentEntity, &childName)) {
        Entity childEntity = rbe_scene_manager_get_entity_child_by_name(parentEntity, childName);
        if (childEntity == NULL_ENTITY) {
            rbe_logger_warn("Failed to get child node from parent entity '%d' with the name '%s'", parentEntity, childName);
            Py_RETURN_NONE;
        }
        return rbe_py_utils_get_entity_instance(childEntity);
    }
    return NULL;
}

PyObject* rbe_py_api_node_get_children(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity parentEntity;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", rbePyApiGenericGetEntityKWList, &parentEntity)) {
        const SceneTreeNode* parentTreeNode = rbe_scene_manager_get_entity_tree_node(parentEntity);
        PyObject* pyChildList = PyList_New(0);
        for (size_t i = 0; i < parentTreeNode->childCount; i++) {
            const SceneTreeNode* childTreeNode = parentTreeNode->children[i];
            PyObject* childNode = rbe_py_utils_get_entity_instance(childTreeNode->entity);
            if (PyList_Append(pyChildList, childNode) == -1) {
                rbe_logger_error("Failed to append entity '%d' to '%d' children list!", parentEntity, childTreeNode->entity);
                PyErr_Print();
            }
        }
        return pyChildList;
    }
    return NULL;
}

PyObject* rbe_py_api_node_get_parent(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity entity;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", rbePyApiGenericGetEntityKWList, &entity)) {
        SceneTreeNode* treeNode = rbe_scene_manager_get_entity_tree_node(entity);
        if (treeNode->parent == NULL) {
            Py_RETURN_NONE;
        }
        return rbe_py_utils_get_entity_instance(treeNode->parent->entity);
    }
    return NULL;
}

// Node2D
PyObject* rbe_py_api_node2D_set_position(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity entity;
    float x;
    float y;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "iff", rbePyApiNode2DSetXYKWList, &entity, &x, &y)) {
        Transform2DComponent* transformComp = (Transform2DComponent*) component_manager_get_component(entity, ComponentDataIndex_TRANSFORM_2D);
        transformComp->localTransform.position.x = x;
        transformComp->localTransform.position.y = y;
        transformComp->isGlobalTransformDirty = true;
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_node2D_add_to_position(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity entity;
    float x;
    float y;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "iff", rbePyApiNode2DSetXYKWList, &entity, &x, &y)) {
        Transform2DComponent* transformComp = (Transform2DComponent*) component_manager_get_component(entity, ComponentDataIndex_TRANSFORM_2D);
        transformComp->localTransform.position.x += x;
        transformComp->localTransform.position.y += y;
        transformComp->isGlobalTransformDirty = true;
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_node2D_get_position(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity entity;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", rbePyApiGenericGetEntityKWList, &entity)) {
        const Transform2DComponent* transformComp = (Transform2DComponent*) component_manager_get_component(entity, ComponentDataIndex_TRANSFORM_2D);
        return Py_BuildValue("(ff)", transformComp->localTransform.position.x, transformComp->localTransform.position.y);
    }
    return NULL;
}

PyObject* rbe_py_api_node2D_get_global_position(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity entity;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", rbePyApiGenericGetEntityKWList, &entity)) {
        Transform2DComponent* transformComp = (Transform2DComponent*) component_manager_get_component(entity, ComponentDataIndex_TRANSFORM_2D);
        TransformModel2D* globalTransform = rbe_scene_manager_get_scene_node_global_transform(entity, transformComp);
        return Py_BuildValue("(ff)", globalTransform->position.x, globalTransform->position.y);
    }
    return NULL;
}

PyObject* rbe_py_api_node2D_set_scale(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity entity;
    float x;
    float y;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "iff", rbePyApiNode2DSetXYKWList, &entity, &x, &y)) {
        Transform2DComponent* transformComp = (Transform2DComponent*) component_manager_get_component(entity, ComponentDataIndex_TRANSFORM_2D);
        transformComp->localTransform.scale.x = x;
        transformComp->localTransform.scale.y = y;
        transformComp->isGlobalTransformDirty = true;
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_node2D_add_to_scale(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity entity;
    float x;
    float y;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "iff", rbePyApiNode2DSetXYKWList, &entity, &x, &y)) {
        Transform2DComponent* transformComp = (Transform2DComponent*) component_manager_get_component(entity, ComponentDataIndex_TRANSFORM_2D);
        transformComp->localTransform.scale.x += x;
        transformComp->localTransform.scale.y += y;
        transformComp->isGlobalTransformDirty = true;
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_node2D_get_scale(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity entity;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", rbePyApiGenericGetEntityKWList, &entity)) {
        const Transform2DComponent* transformComp = (Transform2DComponent*) component_manager_get_component(entity, ComponentDataIndex_TRANSFORM_2D);
        return Py_BuildValue("(ff)", transformComp->localTransform.scale.x, transformComp->localTransform.scale.y);
    }
    return NULL;
}

PyObject* rbe_py_api_node2D_set_rotation(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity entity;
    float rotation;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "if", rbePyApiNode2DSetRotationKWList, &entity, &rotation)) {
        Transform2DComponent* transformComp = (Transform2DComponent*) component_manager_get_component(entity, ComponentDataIndex_TRANSFORM_2D);
        transformComp->localTransform.rotation = rotation;
        transformComp->isGlobalTransformDirty = true;
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_node2D_add_to_rotation(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity entity;
    float rotation;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "if", rbePyApiNode2DSetRotationKWList, &entity, &rotation)) {
        Transform2DComponent* transformComp = (Transform2DComponent*) component_manager_get_component(entity, ComponentDataIndex_TRANSFORM_2D);
        transformComp->localTransform.rotation += rotation;
        transformComp->isGlobalTransformDirty = true;
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_node2D_get_rotation(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity entity;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", rbePyApiGenericGetEntityKWList, &entity)) {
        const Transform2DComponent* transformComp = (Transform2DComponent*) component_manager_get_component(entity, ComponentDataIndex_TRANSFORM_2D);
        return Py_BuildValue("f", transformComp->localTransform.rotation);
    }
    return NULL;
}

// Sprite
PyObject* rbe_py_api_sprite_set_texture(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity entity;
    char* filePath;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "is", rbePyApiSpriteSetTextureKWList, &entity, &filePath)) {
        SpriteComponent* spriteComponent = (SpriteComponent*) component_manager_get_component(entity, ComponentDataIndex_SPRITE);
        RBE_ASSERT_FMT(rbe_asset_manager_has_texture(filePath), "Doesn't have texture with file path '%s'", filePath);
        spriteComponent->texture = rbe_asset_manager_get_texture(filePath);
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_sprite_get_texture(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity entity;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", rbePyApiGenericGetEntityKWList, &entity)) {
        const SpriteComponent* spriteComponent = (SpriteComponent*) component_manager_get_component(entity, ComponentDataIndex_SPRITE);
        return Py_BuildValue("(sssss)", spriteComponent->texture->fileName, "clamp_to_border", "clamp_to_border", "nearest", "nearest");
    }
    return NULL;
}

PyObject* rbe_py_api_sprite_set_draw_source(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity entity;
    float x;
    float y;
    float w;
    float h;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "iffff", rbePyApiGenericSetEntityRectKWList, &entity, &x, &y, &w, &h)) {
        SpriteComponent* spriteComponent = (SpriteComponent*) component_manager_get_component(entity, ComponentDataIndex_SPRITE);
        spriteComponent->drawSource.x = x;
        spriteComponent->drawSource.y = y;
        spriteComponent->drawSource.w = w;
        spriteComponent->drawSource.h = h;
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_sprite_get_draw_source(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity entity;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", rbePyApiGenericGetEntityKWList, &entity)) {
        const SpriteComponent* spriteComponent = (SpriteComponent*) component_manager_get_component(entity, ComponentDataIndex_SPRITE);
        return Py_BuildValue("(ffff)", spriteComponent->drawSource.x, spriteComponent->drawSource.y, spriteComponent->drawSource.w, spriteComponent->drawSource.h);
    }
    return NULL;
}

// Animated Sprite
PyObject* rbe_py_api_animated_sprite_play(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity entity;
    char* animationName;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "is", rbePyApiAnimatedSpriteSetAnimationKWList, &entity, &animationName)) {
        AnimatedSpriteComponent* animatedSpriteComponent = (AnimatedSpriteComponent *) component_manager_get_component(entity, ComponentDataIndex_ANIMATED_SPRITE);
        const bool success = animated_sprite_component_set_animation(animatedSpriteComponent, animationName);
        animatedSpriteComponent->isPlaying = true;
        if (success) {
            Py_RETURN_TRUE;
        }
        Py_RETURN_FALSE;
    }
    return NULL;
}

PyObject* rbe_py_api_animated_sprite_stop(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity entity;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "is", rbePyApiGenericGetEntityKWList, &entity)) {
        AnimatedSpriteComponent* animatedSpriteComponent = (AnimatedSpriteComponent *) component_manager_get_component(entity, ComponentDataIndex_ANIMATED_SPRITE);
        animatedSpriteComponent->isPlaying = false;
    }
    return NULL;
}

// Text Label
PyObject* rbe_py_api_text_label_set_text(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity entity;
    char* text;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "is", rbePyApiTextLabelSetTextKWList, &entity, &text)) {
        TextLabelComponent* textLabelComponent = (TextLabelComponent*) component_manager_get_component(entity, ComponentDataIndex_TEXT_LABEL);
        strcpy(textLabelComponent->text, text);
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_text_label_get_text(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity entity;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", rbePyApiGenericGetEntityKWList, &entity)) {
        TextLabelComponent* textLabelComponent = (TextLabelComponent*) component_manager_get_component(entity, ComponentDataIndex_TEXT_LABEL);
        return Py_BuildValue("s", textLabelComponent->text);
    }
    return NULL;
}

PyObject* rbe_py_api_text_label_set_color(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity entity;
    int red;
    int green;
    int blue;
    int alpha;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "iffff", rbePyApiGenericSetEntityColorKWList, &entity, &red, &green, &blue, &alpha)) {
        TextLabelComponent* textLabelComponent = (TextLabelComponent*) component_manager_get_component(entity, ComponentDataIndex_TEXT_LABEL);
        textLabelComponent->color = rbe_color_get_normalized_color(red, green, blue, alpha);
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_text_label_get_color(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity entity;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", rbePyApiGenericGetEntityKWList, &entity)) {
        TextLabelComponent* textLabelComponent = (TextLabelComponent*) component_manager_get_component(entity, ComponentDataIndex_TEXT_LABEL);
        const int red = (int) (textLabelComponent->color.r * 255.0f);
        const int green = (int) (textLabelComponent->color.r * 255.0f);
        const int blue = (int) (textLabelComponent->color.r * 255.0f);
        const int alpha = (int) (textLabelComponent->color.r * 255.0f);
        return Py_BuildValue("(iiii)", red, green, blue, alpha);
    }
    return NULL;
}

// Collider2D
PyObject* rbe_py_api_collider2D_set_extents(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity entity;
    float w;
    float h;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "iff", rbePyApiGenericSetEntitySize2DKWList, &entity, &w, &h)) {
        Collider2DComponent* collider2DComponent = (Collider2DComponent*) component_manager_get_component(entity, ComponentDataIndex_COLLIDER_2D);
        collider2DComponent->extents.w = w;
        collider2DComponent->extents.h = h;
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_collider2D_get_extents(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity entity;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", rbePyApiGenericGetEntityKWList, &entity)) {
        const Collider2DComponent* collider2DComponent = (Collider2DComponent *) component_manager_get_component(entity, ComponentDataIndex_COLLIDER_2D);
        return Py_BuildValue("(ff)", collider2DComponent->extents.w, collider2DComponent->extents.h);
    }
    return NULL;
}

PyObject* rbe_py_api_collider2D_set_color(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity entity;
    int red;
    int green;
    int blue;
    int alpha;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "iiiii", rbePyApiGenericSetEntityColorKWList, &entity, &red, &green, &blue, &alpha)) {
        Collider2DComponent* collider2DComponent = (Collider2DComponent*) component_manager_get_component(entity, ComponentDataIndex_COLLIDER_2D);
        collider2DComponent->color = rbe_color_get_normalized_color(red, green, blue, alpha);
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_collider2D_get_color(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity entity;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", rbePyApiGenericGetEntityKWList, &entity)) {
        Collider2DComponent* collider2DComponent = (Collider2DComponent *) component_manager_get_component(entity, ComponentDataIndex_COLLIDER_2D);
        const int red = (int) (collider2DComponent->color.r * 255.0f);
        const int green = (int) (collider2DComponent->color.g * 255.0f);
        const int blue = (int) (collider2DComponent->color.b * 255.0f);
        const int alpha = (int) (collider2DComponent->color.a * 255.0f);
        return Py_BuildValue("(iiii)", red, green, blue, alpha);
    }
    return NULL;
}

// ColorSquare
PyObject* rbe_py_api_color_square_set_size(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity entity;
    float w;
    float h;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "iff", rbePyApiGenericSetEntitySize2DKWList, &entity, &w, &h)) {
        ColorSquareComponent* colorSquareComponent = (ColorSquareComponent*) component_manager_get_component(entity, ComponentDataIndex_COLOR_SQUARE);
        colorSquareComponent->size.w = w;
        colorSquareComponent->size.h = h;
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_color_square_get_size(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity entity;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", rbePyApiGenericGetEntityKWList, &entity)) {
        const ColorSquareComponent* colorSquareComponent = (ColorSquareComponent*) component_manager_get_component(entity, ComponentDataIndex_COLOR_SQUARE);
        return Py_BuildValue("(ff)", colorSquareComponent->size.w, colorSquareComponent->size.h);
    }
    return NULL;
}

PyObject* rbe_py_api_color_square_set_color(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity entity;
    int red;
    int green;
    int blue;
    int alpha;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "iiiii", rbePyApiGenericSetEntityColorKWList, &entity, &red, &green, &blue, &alpha)) {
        ColorSquareComponent* colorSquareComponent = (ColorSquareComponent *) component_manager_get_component(entity, ComponentDataIndex_COLOR_SQUARE);
        colorSquareComponent->color = rbe_color_get_normalized_color(red, green, blue, alpha);
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_color_square_get_color(PyObject* self, PyObject* args, PyObject* kwargs) {
    Entity entity;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", rbePyApiGenericGetEntityKWList, &entity)) {
        ColorSquareComponent* colorSquareComponent = (ColorSquareComponent*) component_manager_get_component(entity, ComponentDataIndex_COLOR_SQUARE);
        const int red = (int) (colorSquareComponent->color.r * 255.0f);
        const int green = (int) (colorSquareComponent->color.g * 255.0f);
        const int blue = (int) (colorSquareComponent->color.b * 255.0f);
        const int alpha = (int) (colorSquareComponent->color.a * 255.0f);
        return Py_BuildValue("(iiii)", red, green, blue, alpha);
    }
    return NULL;
}

// Network
PyObject* rbe_py_api_network_is_server(PyObject* self, PyObject* args) {
    if (rbe_network_is_server()) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

// Server
PyObject* rbe_py_api_server_start(PyObject* self, PyObject* args, PyObject* kwargs) {
    int port;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", rbePyApiServerStartKWList, &port)) {
        rbe_udp_server_initialize(port, rbe_ec_system_network_callback);
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_server_stop(PyObject* self, PyObject* args) {
    rbe_udp_server_finalize();
    Py_RETURN_NONE;
}

PyObject* rbe_py_api_server_send(PyObject* self, PyObject* args, PyObject* kwargs) {
    char* message;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "s", rbePyApiNetworkSendMessageKWList, &message)) {
        rbe_udp_server_send_message(message);
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_server_subscribe(PyObject* self, PyObject* args, PyObject* kwargs) {
    char* signalId;
    Entity listenerNode;
    PyObject* listenerFunc;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "siO", rbePyApiNetworkSubscribeKWList, &signalId, &listenerNode, &listenerFunc)) {
        RBE_ASSERT(PyObject_IsTrue(listenerFunc));
        const RBEScriptContext* scriptContext =  rbe_py_get_script_context();
        RBE_ASSERT(scriptContext != NULL && scriptContext->on_entity_subscribe_to_network_callback != NULL);
        scriptContext->on_entity_subscribe_to_network_callback(listenerNode, listenerFunc, signalId);

        Py_DECREF(listenerFunc);
        Py_RETURN_NONE;
    }
    return NULL;
}

// Client
PyObject* rbe_py_api_client_start(PyObject* self, PyObject* args, PyObject* kwargs) {
    char* host;
    int port;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "si", rbePyApiClientStartKWList, &host, &port)) {
        rbe_udp_client_initialize(host, port, rbe_ec_system_network_callback);
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_client_stop(PyObject* self, PyObject* args) {
    rbe_udp_client_finalize();
    Py_RETURN_NONE;
}

PyObject* rbe_py_api_client_send(PyObject* self, PyObject* args, PyObject* kwargs) {
    char* message;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "s", rbePyApiNetworkSendMessageKWList, &message)) {
        rbe_udp_client_send_message(message);
        Py_RETURN_NONE;
    }
    return NULL;
}

PyObject* rbe_py_api_client_subscribe(PyObject* self, PyObject* args, PyObject* kwargs) {
    char* signalId;
    Entity listenerNode;
    PyObject* listenerFunc;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "siO", rbePyApiNetworkSubscribeKWList, &signalId, &listenerNode, &listenerFunc)) {
        RBE_ASSERT(PyObject_IsTrue(listenerFunc));
        const RBEScriptContext* scriptContext =  rbe_py_get_script_context();
        RBE_ASSERT(scriptContext != NULL && scriptContext->on_entity_subscribe_to_network_callback != NULL);
        scriptContext->on_entity_subscribe_to_network_callback(listenerNode, listenerFunc, signalId);

        Py_DECREF(listenerFunc);
        Py_RETURN_NONE;
    }
    return NULL;
}

// Collision Handler
PyObject* rbe_py_api_collision_handler_process_collisions(PyObject* self, PyObject* args, PyObject* kwargs) {
#define TYPE_BUFFER_SIZE 32
    Entity entity;
    if (PyArg_ParseTupleAndKeywords(args, kwargs, "i", rbePyApiGenericGetEntityKWList, &entity)) {
        char typeBuffer[TYPE_BUFFER_SIZE];
        PyObject* pyCollidedEntityList = PyList_New(0);
        CollisionResult collisionResult = rbe_collision_process_entity_collisions(entity);
        for (size_t i = 0; i < collisionResult.collidedEntityCount; i++) {
            const Entity collidedEntity = collisionResult.collidedEntities[i];
            NodeComponent* nodeComponent = (NodeComponent*) component_manager_get_component(collidedEntity, ComponentDataIndex_NODE);
            strcpy(typeBuffer, node_get_component_type_string(nodeComponent->type));
            if (PyList_Append(pyCollidedEntityList, Py_BuildValue("(is)", collidedEntity, typeBuffer)) == -1) {
                PyErr_Print();
            }
        }
        return Py_BuildValue("O", pyCollidedEntityList);
    }
    return NULL;
#undef TYPE_BUFFER_SIZE
}
