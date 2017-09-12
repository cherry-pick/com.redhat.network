io_systemd_network_sources = files('''
        main.c
'''.split())

io_systemd_network_varlink_c_inc = custom_target(
        'io.systemd.network.varlink',
        input : 'io.systemd.network.varlink',
        output : 'io.systemd.network.varlink.c.inc',
        command : [varlink_wrapper_py, '@INPUT@', '@OUTPUT@'])

exe = executable(
        'io.systemd.network',
        io_systemd_network_sources,
        io_systemd_network_varlink_c_inc,
        dependencies : [
                libvarlink,
                libnl,
                libnlroute
        ],
        install : true)
