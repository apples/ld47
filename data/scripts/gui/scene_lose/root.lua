local vdom = require('vdom')

-- goto_menu: () => ()
return function(props)
    return vdom.create_element('panel', { width = '100%', height = '100%', texture = 'background' },
        vdom.create_element('panel', { width = 800, height = 48, halign = 'center', bottom = 600, color = '#0008' },
            vdom.create_element(
                'label',
                {
                    halign = 'center',
                    valign = 'top',
                    top = 9,
                    color = '#fff',
                    text = 'You have lost...',
                    height = 24,
                }
            )
        ),
        vdom.create_element('panel', { width = 200, height = 48, halign = 'center', bottom = 500, color = '#0008' },
            vdom.create_element(
                'label',
                {
                    halign = 'center',
                    valign = 'top',
                    top = 8,
                    color = '#fff',
                    text = '[Return to main menu]',
                    height = 24,
                    on_click = props.goto_menu,
                }
            )
        )
    )
end
