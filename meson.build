project('elfutils', 'c',
  version: '0.186',
  default_options: [
    'c_std=gnu99',
  ],
)

eu_version = 186

cc = meson.get_compiler('c')

have_msvc = cc.get_id() == 'msvc'

threads_dep = dependency('threads')
zlib_dep = dependency('zlib')

cdata = configuration_data()

project_name = meson.project_name()
project_version = meson.project_version()
cdata.set_quoted('PACKAGE', project_name)
cdata.set_quoted('PACKAGE_NAME', project_name)
cdata.set_quoted('PACKAGE_TARNAME', project_name)
cdata.set_quoted('PACKAGE_VERSION', project_version)
cdata.set_quoted('VERSION', project_version)
cdata.set_quoted('PACKAGE_STRING', project_name + ' ' + project_version)
cdata.set_quoted('PACKAGE_URL', 'http://elfutils.org/')
cdata.set_quoted('PACKAGE_BUGREPORT', 'https://sourceware.org/bugzilla')

cdata.set('SIZEOF_LONG', cc.sizeof('long'))

newline_for_old_gcc_versions = '\n'

if cc.has_function_attribute('visibility:hidden')
  cdata.set('HAVE_VISIBILITY', 1)
endif

if cc.compiles('struct test { int x; } __attribute__((gcc_struct));' + newline_for_old_gcc_versions,
               args: ['-Werror'],
               name: 'compiler supports __attribute__((gcc_struct))')
  cdata.set('HAVE_GCC_STRUCT', 1)
endif

if cc.compiles('''int
                  foo (int a)
                  {
                    int result = 0;
                    switch (a)
                      {
                      case 42:
                        result += 2;
                        __attribute__ ((fallthrough));
                      default:
                        result += 1;
                      }
                    return result;

                    return a;
                  }''' + newline_for_old_gcc_versions,
               args: ['-Werror'],
               name: 'compiler supports __attribute__((fallthrough))')
  cdata.set('HAVE_FALLTHROUGH', 1)
endif

if cc.compiles('__thread int global_error;' + newline_for_old_gcc_versions,
               name: 'compiler supports __thread storage class')
  cdata.set('HAVE_THREAD_STORAGE_CLASS', 1)
endif

if cc.has_function('__builtin_bswap64')
  cdata.set('HAVE_BSWAP64', 1)
endif

cdata.set('STDC_HEADERS', 1)
cdata.set('DEFAULT_AR_DETERMINISTIC', 'false')
cdata.set10('CHECK_UNDEFINED', 0)
cdata.set('USE_ZLIB', 1)

system_headers = [
  'byteswap.h',
  'endian.h',
  'error.h',
  'err.h',
  'inttypes.h',
  'memory.h',
  'stdint.h',
  'stdlib.h',
  'strings.h',
  'string.h',
  'sys/stat.h',
  'sys/types.h',
  'unistd.h',
]
foreach h : system_headers
  if cc.has_header(h)
    cdata.set('HAVE_' + h.underscorify().to_upper(), 1)
  endif
endforeach

cdata.set10('HAVE_STDATOMIC_H', cc.has_header('stdatomic.h'))

gnu_source_args = ['-D_GNU_SOURCE']
cdata.set10('HAVE_DECL_MEMRCHR', cc.has_header_symbol('string.h', 'memrchr', args: gnu_source_args))
cdata.set10('HAVE_DECL_RAWMEMCHR', cc.has_header_symbol('string.h', 'rawmemchr', args: gnu_source_args))
cdata.set10('HAVE_DECL_POWEROF2', cc.has_header_symbol('sys/param.h', 'powerof2'))
cdata.set10('HAVE_DECL_MEMPCPY', cc.has_header_symbol('string.h', 'mempcpy', args: gnu_source_args))
cdata.set10('HAVE_DECL_REALLOCARRAY', cc.has_header_symbol('stdlib.h', 'reallocarray', args: gnu_source_args))

system_functions = [
  'mremap',
  'process_vm_readv',
]
foreach f : system_functions
  if cc.has_function(f)
    cdata.set('HAVE_' + h.to_upper(), 1)
  endif
endforeach

have_strerror_r = cc.has_header_symbol('string.h', 'strerror_r', args: gnu_source_args)
cdata.set10('HAVE_DECL_STRERROR_R', have_strerror_r)
if have_strerror_r
  cdata.set10('HAVE_STRERROR_R', have_strerror_r)
  if cc.compiles('''#include <errno.h>
                    #include <stdio.h>
                    #include <string.h>

                    void
                    foo (void)
                    {
                      char buf[512];
                      char *str = strerror_r (errno, buf, sizeof (buf));
                      puts (str);
                    }''' + newline_for_old_gcc_versions,
                 name: 'strerror_r returns char *',
                 args: ['-Werror'] + gnu_source_args)
    cdata.set('STRERROR_R_CHAR_P', 1)
  endif
endif

if cc.has_type('struct user_regs_struct',
               prefix: '''#include <sys/ptrace.h>
                          #include <sys/time.h>
                          #include <sys/user.h>''')
  cdata.set('HAVE_SYS_USER_REGS', 1)
endif

if cc.has_function('pthread_setname_np', dependencies: threads_dep)
  cdata.set('HAVE_PTHREAD_SETNAME_NP', 1)
endif

configure_file(
  input: 'config.h.in.meson',
  output: 'config.h',
  configuration: cdata,
)
add_project_arguments(
  have_msvc ? '/FI' : '-include', meson.current_build_dir() / 'config.h',
  '-D_GNU_SOURCE=1',
  language: 'c',
)

configure_file(
  input: 'config' / 'version.h.in',
  output: 'version.h',
  configuration: {'eu_version': eu_version},
  install: true,
  install_dir: get_option('includedir') / 'elfutils'
)

elfutils_incdirs = include_directories(
  'lib',
)

subdir('libelf')
