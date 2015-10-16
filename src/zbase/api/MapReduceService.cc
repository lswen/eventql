/**
 * Copyright (c) 2015 - The CM Authors <legal@clickmatcher.com>
 *   All Rights Reserved.
 *
 * This file is CONFIDENTIAL -- Distribution or duplication of this material or
 * the information contained herein is strictly forbidden unless prior written
 * permission is obtained.
 */
#include "zbase/api/MapReduceService.h"
#include "stx/protobuf/MessageDecoder.h"
#include "stx/protobuf/JSONEncoder.h"

using namespace stx;

namespace zbase {

MapReduceService::MapReduceService(
    ConfigDirectory* cdir,
    AnalyticsAuth* auth,
    zbase::TSDBService* tsdb,
    zbase::PartitionMap* pmap,
    zbase::ReplicationScheme* repl,
    JSRuntime* js_runtime) :
    cdir_(cdir),
    auth_(auth),
    tsdb_(tsdb),
    pmap_(pmap),
    repl_(repl),
    js_runtime_(js_runtime) {}

void MapReduceService::mapPartition(
    const AnalyticsSession& session,
    const String& table_name,
    const SHA1Hash& partition_key) {
  logDebug(
      "z1.mapreduce",
      "Executing map shard; partition=$0/$1/$2 output=$3",
      session.customer(),
      table_name,
      partition_key.toString());

  auto table = pmap_->findTable(
      session.customer(),
      table_name);

  if (table.isEmpty()) {
    RAISEF(
        kNotFoundError,
        "table not found: $0/$1/$2",
        session.customer(),
        table_name);
  }

  auto partition = pmap_->findPartition(
      session.customer(),
      table_name,
      partition_key);

  if (partition.isEmpty()) {
    RAISEF(
        kNotFoundError,
        "partition not found: $0/$1/$2",
        session.customer(),
        table_name,
        partition_key.toString());
  }

  auto schema = table.get()->schema();
  auto reader = partition.get()->getReader();

  //jsval json = JSVAL_NULL;

  auto js_ctx = mkRef(new JavaScriptContext());
  js_ctx->loadProgram(
      R"(
        function mymapper(obj) {
          return "blah: " + obj;
        }
      )");

  reader->fetchRecords([&schema, &js_ctx] (const Buffer& record) {
    msg::MessageObject msgobj;
    msg::MessageDecoder::decode(record, *schema, &msgobj);
    Buffer msgjson;
    json::JSONOutputStream msgjsons(BufferOutputStream::fromBuffer(&msgjson));
    msg::JSONEncoder::encode(msgobj, *schema, &msgjsons);

    js_ctx->callMapFunction("mymapper", msgjson.toString());
  });
}

} // namespace zbase
