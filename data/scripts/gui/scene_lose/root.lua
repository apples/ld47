local vdom = require('vdom')

-- goto_menu: () => ()
-- enemies_spawned: number
-- turn_count: number
-- dagrons_defeated: number
return function(props)
    return vdom.create_element('panel', { width = '100%', height = '100%', texture = 'background' },
        vdom.create_element('panel', { width = 800, height = 160, halign = 'center', bottom = 600, color = '#0008' },
            vdom.create_element(
                'label',
                {
                    halign = 'center',
                    valign = 'top',
                    top = 8,
                    color = '#fff',
                    text = 'You have lost...',
                    height = 24,
                }
            ),
            vdom.create_element(
                'label',
                {
                    halign = 'center',
                    valign = 'top',
                    top = 32,
                    color = '#fff',
                    text = 'You saw '..props.enemies_spawned..' enemies.',
                    height = 24,
                }
            ),
            vdom.create_element(
                'label',
                {
                    halign = 'center',
                    valign = 'top',
                    top = 56,
                    color = '#fff',
                    text = 'You passed '..props.turn_count..' turns.',
                    height = 24,
                }
            ),
            vdom.create_element(
                'label',
                {
                    halign = 'center',
                    valign = 'top',
                    top = 80,
                    color = '#fff',
                    text = 'YOU DEFEATED '..props.dagrons_defeated..' DAGRONS.',
                    height = 24,
                }
            ),
            vdom.create_element(
                'label',
                {
                    halign = 'center',
                    valign = 'top',
                    top = 128,
                    color = '#fff',
                    text = '[Return to main menu]',
                    height = 24,
                    on_click = props.goto_menu,
                }
            )
        )
    )
end
