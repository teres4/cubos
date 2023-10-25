#include <cubos/engine/cubos.hpp>
#include <cubos/engine/voxels/palette.hpp>
#include <cubos/engine/voxels/plugin.hpp>
#include <cubos/engine/renderer/plugin.hpp>
#include <cubos/engine/voxels/grid.hpp>
#include <cubos/engine/renderer/point_light.hpp>
#include <cubos/engine/settings/settings.hpp>

#include <cubos/engine/transform/plugin.hpp>

//ex 2
#include <cubos/engine/assets/assets.hpp>
#include <cubos/engine/assets/plugin.hpp>
#include <cubos/engine/assets/bridges/file.hpp>

#include <cubos/engine/settings/plugin.hpp>


#include <cubos/engine/assets/bridges/binary.hpp>

//ex3

#include <cubos/engine/input/bindings.hpp>
#include <cubos/engine/input/plugin.hpp>

#include <cubos/core/ecs/system.hpp>
#include <cubos/core/ecs/query.hpp>

#include <cubos/core/data/old/debug_serializer.hpp> //(??)

#include <cubos/core/memory/stream.hpp>


#include <cubos/engine/scene/plugin.hpp>


using namespace cubos::engine;

using cubos::core::data::old::Debug;

using cubos::core::ecs::Read;
using cubos::core::ecs::Write;
using cubos::core::io::Window;
using cubos::core::ecs::Query;
using cubos::core::ecs::Commands;



static const Asset<VoxelPalette> PaletteAsset = AnyAsset("798f5af2-70da-11ee-b962-0242ac120002");
//static const Asset<VoxelGrid> CastleAsset = AnyAsset("3824aec8-70da-11ee-b962-0242ac120002");
static const Asset<InputBindings> BindingsAsset = AnyAsset("bf49ba61-5103-41bc-92e0-8a442d7842c3");
static const Asset<Scene> SceneAsset = AnyAsset("dfd99016-893f-4c5b-9548-3e367f0b4d07");


static void spawnScene(Commands commands, Read<Assets> assets)
{
    auto sceneRead = assets->read(SceneAsset);
    commands.spawn(sceneRead->blueprint);
}

/*
From ex1
static void setPaletteSystem(Write<Renderer> renderer)
{
    // Create a simple palette with 3 materials (red, green and blue).
    (*renderer)->setPalette(VoxelPalette{{
        {{0.95, 0.4 , 0.6, 1}},
        {{0.1, 0.1, 1, 1}},
        {{0.4, 0.1, 0.95, 1}},
    }});
}*/



/*
From ex 1
static void spawnVoxelGridSystem(Commands commands, Write<Assets> assets)
{
    // Create a 2x2x2 grid whose voxels alternate between the materials defined in the palette.
    auto gridAsset = assets->create(VoxelGrid{{2, 2, 2}, {1, 2, 3, 1, 2, 3, 1, 2}});

    // Spawn an entity with a renderable grid component and a identity transform.
    commands.create(RenderableGrid{gridAsset, {-1.0F, 0.0F, -1.0F}}, LocalToWorld{});
}*/



static void spawnLightSystem(Commands commands)
{
    // Spawn a point light.
    commands.create()
        .add(PointLight{.color = {1.0F, 1.0F, 1.0F}, .intensity = 3.0F, .range = 50.0F})
        .add(Position{{1.0F, 20.0F, -2.0F}});
}


static void spawnCamerasSystem(Commands commands, Write<ActiveCameras> camera)
{
    // Spawn the a camera entity for the first viewport.
    camera->entities[0] =
        commands.create()
            .add(Camera{.fovY = 60.0F, .zNear = 0.1F, .zFar = 1000.0F})
            .add(Position{{50.0F, 50.0F, 50.0F}})
            .add(Rotation{glm::quatLookAt(glm::normalize(glm::vec3{-1.0F, -1.0F, -1.0F}), glm::vec3{0.0F, 1.0F, 0.0F})})
            .entity();
}



static void loadPaletteSystem(Read<Assets> assets, Write<Renderer> renderer)
{   

    auto palette = assets->read(PaletteAsset);
    (*renderer)->setPalette(*palette);

}

static void settingsSystem(Write<Settings> settings)
{
    settings->setString("assets.io.path", SAMPLE_ASSETS_FOLDER);
    
}




/*From ex3
static void spawnCastleSystem(Commands cmds, Read<Assets> assets)
{   

    // Calculate the necessary offset to center the model on (0, 0, 0).
    auto castle = assets->read(CastleAsset);
    glm::vec3 offset = glm::vec3(castle->size().x, 0.0F, castle->size().z) / -2.0F;

    // Create the car entity
    
    cmds.create().add(RenderableGrid{CastleAsset, offset}).add(LocalToWorld{}).add(Position{{0.0F, 0.0F, 0.0F}});
}*/


static void init(Read<Assets> assets, Write<Input> input)
{
    auto bindings = assets->read<InputBindings>(BindingsAsset);
    input->bind(*bindings);
    CUBOS_INFO("Loaded bindings: {}", Debug(input->bindings().at(0)));
}



static void Move(Query<Write<Position>, Write<RenderableGrid>> query, Read<Input> input){

    glm::vec3 vec{-1.0f, 1.0f, -5.0f };

    if(input->pressed("X")){
        CUBOS_INFO("X");        
        for(auto[entity, position, grid]: query)
        {
            position->vec += vec;
        }
    }
    if(input->pressed("Z")){
        CUBOS_INFO("Z");  
        for(auto[entity, position, grid]: query)
        {
            position->vec += vec * -1.0f;
        }
    }
}

int main()
{
    Cubos cubos{};

    cubos.addPlugin(assetsPlugin);
    cubos.addPlugin(voxelsPlugin);
    cubos.addPlugin(rendererPlugin);
    cubos.addPlugin(inputPlugin);
    cubos.addPlugin(scenePlugin);

    cubos.startupSystem(settingsSystem).tagged("cubos.settings");
    cubos.startupSystem(spawnCamerasSystem);
    //cubos.startupSystem(spawnCastleSystem).tagged("cubos.assets");
    cubos.startupSystem(spawnLightSystem);
    cubos.startupSystem(spawnScene).tagged("spawn").tagged("cubos.assets");
    cubos.startupSystem(init).tagged("cubos.assets");
    cubos.startupSystem(loadPaletteSystem).tagged("cubos.assets").after("cubos.renderer.init");
    cubos.system(Move).after("cubos.input.update");
    

    //cubos.startupSystem(setPaletteSystem).after("cubos.renderer.init");
    //cubos.startupSystem(spawnVoxelGridSystem);

    cubos.run();
}
