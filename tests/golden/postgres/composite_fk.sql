-- ------------------------------------------------------
-- beDBug forward engineering script
-- Target engine: PostgreSQL
-- ------------------------------------------------------

CREATE TABLE "public"."invoice" (
  "country" char(2) NOT NULL,
  "number" integer NOT NULL,
  "issued_on" timestamptz,
  PRIMARY KEY ("country", "number")
);

CREATE TABLE "public"."invoice_lines" (
  "invoice_country" char(2) NOT NULL,
  "invoice_number" integer NOT NULL,
  "sku" varchar(32) NOT NULL,
  "qty" integer NOT NULL
);

ALTER TABLE "public"."invoice_lines"
  ADD CONSTRAINT "fk_lines_invoice" FOREIGN KEY ("invoice_country", "invoice_number")
  REFERENCES "public"."invoice" ("country", "number");

CREATE INDEX "ix_lines_sku" ON "public"."invoice_lines" ("sku");
