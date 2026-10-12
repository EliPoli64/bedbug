-- ------------------------------------------------------
-- bedbug forward engineering script
-- Target engine: PostgreSQL
-- ------------------------------------------------------

CREATE TYPE "types_demo_c_enum_enum" AS ENUM ('red', 'green', 'blue');

CREATE TABLE "public"."types_demo" (
  "c_int" integer,
  "c_bigint" bigint,
  "c_smallint" smallint,
  "c_varchar" varchar(100),
  "c_char" char(2),
  "c_text" text,
  "c_bool" boolean,
  "c_date" date,
  "c_time" time,
  "c_ts" timestamptz,
  "c_dec" numeric(12,3),
  "c_float" real,
  "c_double" double precision,
  "c_blob" bytea,
  "c_json" jsonb,
  "c_uuid" uuid,
  "c_enum" types_demo_c_enum_enum,
  "weird""name" varchar(5)
);

COMMENT ON TABLE "public"."types_demo" IS 'Every canonical type in one place.';
