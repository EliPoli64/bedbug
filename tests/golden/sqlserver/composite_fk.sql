-- ------------------------------------------------------
-- beDBug forward engineering script
-- Target engine: SQL Server
-- ------------------------------------------------------

CREATE TABLE [public].[invoice] (
  [country] nchar(2) NOT NULL,
  [number] int NOT NULL,
  [issued_on] datetime2,
  PRIMARY KEY ([country], [number])
);

CREATE TABLE [public].[invoice_lines] (
  [invoice_country] nchar(2) NOT NULL,
  [invoice_number] int NOT NULL,
  [sku] nvarchar(32) NOT NULL,
  [qty] int NOT NULL
);

ALTER TABLE [public].[invoice_lines]
  ADD CONSTRAINT [fk_lines_invoice] FOREIGN KEY ([invoice_country], [invoice_number])
  REFERENCES [public].[invoice] ([country], [number]);

CREATE INDEX [ix_lines_sku] ON [public].[invoice_lines] ([sku]);
