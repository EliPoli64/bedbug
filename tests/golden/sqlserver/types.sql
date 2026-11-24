-- ------------------------------------------------------
-- beDBug forward engineering script
-- Target engine: SQL Server
-- ------------------------------------------------------

CREATE TABLE [public].[types_demo] (
  [c_int] int,
  [c_bigint] bigint,
  [c_smallint] smallint,
  [c_varchar] nvarchar(100),
  [c_char] nchar(2),
  [c_text] nvarchar(max),
  [c_bool] bit,
  [c_date] date,
  [c_time] time,
  [c_ts] datetime2,
  [c_dec] decimal(12,3),
  [c_float] real,
  [c_double] float,
  [c_blob] varbinary(max),
  [c_json] nvarchar(max),
  [c_uuid] uniqueidentifier,
  [c_enum] nvarchar(255),
  [weird"name] nvarchar(5)
);
