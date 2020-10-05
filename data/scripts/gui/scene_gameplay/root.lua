local vdom = require('vdom')
local linq = require('linq')

return function(props)
    local turn_name = 'None'
    local explanation = {}

    if (props.turn == 0) then turn_name = 'SUMMON' explanation = {'Drag a movement card onto a fighter, or press Space to skip.'}
    elseif (props.turn == 1) then turn_name = 'SET_ACTIONS' explanation = {'Toggle the action state of your fighters by clicking on them.', 'Press Space when done.'}
    elseif (props.turn == 2) then turn_name = 'AUTOPLAYER'
    elseif (props.turn == 3) then turn_name = 'ENEMY_SPAWN'
    elseif (props.turn == 4) then turn_name = 'ENEMY_MOVE'
    elseif (props.turn == 5) then turn_name = 'ATTACK'
    end

    for i,v in ipairs(explanation) do
        explanation[i] = vdom.create_element(
            'label',
            {
                halign='left',
                valign='top',
                top=80 + i * 24,
                height = 24,
                text = v,
                color = '#fff'
            }
        )
    end

    return vdom.create_element('widget', { width = '100%', height = '100%' },
        vdom.create_element(
            'label',
            {
                halign='left',
                valign='top',
                top=0,
                height = 24,
                text = 'Turn: ' .. turn_name,
                color = '#fff'
            }
        ),
        explanation
    )
end
