local scene = {}

function scene.init()
    print('Initializing gameplay...')

    local test = entities:create_entity()

    local xf = component.transform.new()
    xf.pos.x = 8
    xf.pos.y = 4.5
    xf.pos.z = 1

    local s = component.sprite.new()
    s.texture = "kagami"
    s.frames:add(0)

    entities:add_component(test, xf)
    entities:add_component(test, s)

    print('Initialized gameplay.')
end

return scene
