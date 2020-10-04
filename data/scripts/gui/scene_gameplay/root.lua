local vdom = require('vdom')

return function(props)
    local turn_name = 'None';

    if (props.turn == 0) then turn_name = 'SUMMON'
    elseif (props.turn == 1) then turn_name = 'SET_ACTIONS'
    elseif (props.turn == 2) then turn_name = 'AUTOPLAYER'
    elseif (props.turn == 3) then turn_name = 'ENEMY_SPAWN'
    elseif (props.turn == 4) then turn_name = 'ENEMY_MOVE'
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
        )
    )
end
