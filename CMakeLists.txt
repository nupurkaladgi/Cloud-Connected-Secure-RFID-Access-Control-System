idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    EMBED_TXTFILES
        "certs/AmazonRootCA.pem"
        "certs/certificate.pem.crt"
        "certs/private.pem.key"
)