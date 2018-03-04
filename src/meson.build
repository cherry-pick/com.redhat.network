com_redhat_network_sources = files('''
        main.c
'''.split())

com_redhat_network_varlink_c_inc = custom_target(
        'com_redhat.network.varlink',
        input : 'com.redhat.network.varlink',
        output : 'com.redhat.network.varlink.c.inc',
        command : [varlink_wrapper_py, '@INPUT@', '@OUTPUT@'])

exe = executable(
        'com.redhat.network',
        com_redhat_network_sources,
        com_redhat_network_varlink_c_inc,
        dependencies : [
                libvarlink,
                libnl,
                libnlroute
        ],
        install : true)
