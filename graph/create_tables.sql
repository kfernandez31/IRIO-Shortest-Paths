DROP TABLE IF EXISTS optimal_connection;
DROP TABLE IF EXISTS direct_connection;
DROP TABLE IF EXISTS edge;
DROP TABLE IF EXISTS node;

CREATE TABLE node (
    id          BIGINT           NOT NULL,
    lon         DOUBLE PRECISION NOT NULL,
    lat         DOUBLE PRECISION NOT NULL,
    region      INTEGER          NOT NULL,

    PRIMARY KEY (id)
);

CREATE TABLE edge (
    source       BIGINT NOT NULL,
    target       BIGINT NOT NULL,
    length       REAL   NOT NULL,
    
    FOREIGN KEY (source) REFERENCES node(id),
    FOREIGN KEY (target) REFERENCES node(id),

    PRIMARY KEY (source, target)
);

CREATE TABLE optimal_connection (
    source      BIGINT     NOT NULL,
    target      BIGINT     NOT NULL,
    region      INTEGER    NOT NULL,
    
    FOREIGN KEY (source, target) REFERENCES edge(source, target),
    PRIMARY KEY (source, target, region)
)

CREATE TABLE direct_connection (
    region_1    INTEGER     NOT NULL,
    region_2    INTEGER     NOT NULL,
    
    PRIMARY KEY (region_1, region_2)
)
