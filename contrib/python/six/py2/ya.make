# Generated by devtools/yamaker (pypi).

PY2_LIBRARY()

VERSION(1.17.0)

LICENSE(MIT)

NO_LINT()

PY_SRCS(
    TOP_LEVEL
    six.py
)

RESOURCE_FILES(
    PREFIX contrib/python/six/py2/
    .dist-info/METADATA
    .dist-info/top_level.txt
)

END()

RECURSE_FOR_TESTS(
    tests
)
