# Generated by devtools/yamaker from nixpkgs 24.05.

LIBRARY()

LICENSE(BSL-1.0)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

VERSION(1.88.0)

ORIGINAL_SOURCE(https://github.com/boostorg/context/archive/boost-1.88.0.tar.gz)

IF (SANITIZER_TYPE)
    PEERDIR(
        contrib/restricted/boost/context/ucontext_impl
    )
ELSE()
    PEERDIR(
        contrib/restricted/boost/context/fcontext_impl
    )
ENDIF()

ADDINCL(
    GLOBAL contrib/restricted/boost/context/include
)

END()

RECURSE(
    fcontext_impl
    impl_common
    ucontext_impl
)
