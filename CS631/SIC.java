
import com.google.gson.Gson;
import com.google.gson.JsonParser;
import jdk.nashorn.internal.parser.JSONParser;
import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.hbase.*;
import org.apache.hadoop.hbase.client.*;
import org.apache.hadoop.hbase.coprocessor.ObserverContext;
import org.apache.hadoop.hbase.coprocessor.RegionCoprocessor;
import org.apache.hadoop.hbase.coprocessor.RegionCoprocessorEnvironment;
import org.apache.hadoop.hbase.coprocessor.RegionObserver;
import org.apache.hadoop.hbase.filter.*;
import org.apache.hadoop.hbase.filter.RowFilter;
import org.apache.hadoop.hbase.regionserver.InternalScanner;
import org.apache.hadoop.hbase.regionserver.RegionScanner;
import org.apache.hadoop.hbase.util.Bytes;
import org.apache.hadoop.hbase.wal.WALEdit;
import org.codehaus.jettison.json.JSONArray;
import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;
import org.mortbay.log.Log;
import org.mortbay.util.ajax.JSON;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;


import javax.swing.*;
import java.io.IOException;
import java.util.*;
import java.util.regex.Matcher;
import java.util.regex.Pattern;


public class SIC implements RegionObserver, RegionCoprocessor {

    String INDEX_TABLE = "";
    String SOURCE_TABLE = "";
    String CF = "";
    String INDEX_COLUMN = "";
    String SOURCE_COLUMN = "";
    boolean debug = false;
    String workerRegion;
    String scanString = "";
    JSONObject obj;
    JSONObject families;
    JSONArray columns;
    ArrayList<String> tokens;
    String filter_string;

    private static final Logger LOG = LoggerFactory.getLogger(SIC.class);

    @Override
    public void start(CoprocessorEnvironment env) throws IOException {

        try {
           if (debug) LOG.info("(start)");
            SOURCE_TABLE = env.getConfiguration().get("arg1");
            CF = env.getConfiguration().get("arg2");

            INDEX_COLUMN = env.getConfiguration().get("arg3");
            SOURCE_COLUMN = env.getConfiguration().get("arg4");
            INDEX_TABLE = SOURCE_TABLE + "_" + INDEX_COLUMN;
            if (debug)
                LOG.info("SOURCE_TABLE : " + SOURCE_TABLE + " INDEX_TABLE: " + INDEX_TABLE + " CF: " + CF + " INDEX_COLUMN: " + INDEX_COLUMN + " SOURCE_COLUMN:" + SOURCE_COLUMN);
            TableName tableName = TableName.valueOf(INDEX_TABLE);
            ColumnFamilyDescriptorBuilder columnDescBuilder = ColumnFamilyDescriptorBuilder
                    .newBuilder(Bytes.toBytes(CF));

            Configuration conf = HBaseConfiguration.create();
            Connection connection = ConnectionFactory.createConnection(conf);

            Admin admin = connection.getAdmin();
            List<RegionInfo> regions = admin.getRegions(TableName.valueOf(SOURCE_TABLE));
            LOG.info("start::region" + regions.toString());
            workerRegion = Bytes.toString(regions.get(0).getRegionName());
            LOG.info("start:: region size" + regions.size());
            LOG.info("start::" + admin.getRegions(TableName.valueOf(SOURCE_TABLE)).toString());
            TableDescriptorBuilder tableDescBuilder = TableDescriptorBuilder.newBuilder(tableName);
            tableDescBuilder.setColumnFamily(columnDescBuilder.build());
            TableDescriptor tableDescriptor = tableDescBuilder.build();
            admin.createTable(tableDescriptor);



        } catch (Exception e) {
            if (debug) LOG.info(e.getMessage());
        }
    }

    @Override
    public Optional<RegionObserver> getRegionObserver() {
        return Optional.of(this);
    }

    @Override
    public void postPut(ObserverContext<RegionCoprocessorEnvironment> c, Put put, WALEdit edit, Durability durability) throws IOException {
        try {
            Table table = c.getEnvironment().getConnection().getTable(TableName.valueOf(INDEX_TABLE));
            byte[] rowkey = put.getRow();
            LOG.info("postPut::" + put.toJSON());
            final List<Cell> name_data_item = put.get(Bytes.toBytes(CF), Bytes.toBytes((INDEX_COLUMN)));
            Cell first_name = name_data_item.get(0);
            Put p = new Put(Bytes.add(first_name.getValueArray(), rowkey));
            p.addColumn(Bytes.toBytes(CF), Bytes.toBytes(SOURCE_COLUMN),
                    rowkey);
            table.put(p);
            table.close();
        } catch (IOException e) {
            e.printStackTrace();
        }

    }

    @Override
    public void preScannerOpen(ObserverContext<RegionCoprocessorEnvironment> c, Scan scan) throws IOException {

        scanString = scan.toString();
        try {
            obj = new JSONObject(scanString);
            families = (JSONObject) obj.get("families");
            LOG.info("preScannerOpen::lenght : " + families.length());
            LOG.info("preScannerOpen::families : " + families.toString());
            columns = families.getJSONArray(CF);

            LOG.info("preScannerOpen::JSON Array: " + columns.toString());
            LOG.info(INDEX_COLUMN);
            int totalColumn = (int) obj.get("totalColumns");
            LOG.info("preScannerOpen::totalCOULMS: " + totalColumn);
            LOG.info("preScannerOpen::FAMILY size: " + columns.length());

            if(families.length()>1 && columns.length()>1){
                return;
            }
            filter_string = (String) obj.get("filter");

            if( (tokens = isValueFilter(filter_string))!=null){
                if(!tokens.get(0).equals("ValueFilter") && !tokens.get(1).equals("EQUAL") && !columns.get(0).toString().equals(INDEX_COLUMN)) {
                    LOG.info("preScannerOpen::  Not using secndary index");
                    return;
                }
            }

            //SingleColumnValueFilter (cf, first_name, EQUAL, Kenny)
            else if((tokens = isSingleValueFilter(filter_string))!=null && tokens.get(3).equals("EQUAL")){
                if(!tokens.get(0).equals("SingleColumnValueFilter")){
                    LOG.info(" preScannerOpen::  Not using secndary index");
                    return;
                }
            }
            else{
                LOG.info("preScannerOpen:: not using secndary index else");
                return;
            }

            Filter prefixFiltertemp = new PrefixFilter(Bytes.toBytes("-123-"));
            scan.setFilter(prefixFiltertemp);
            scan.setLimit(0);
            LOG.info("Scan Filter : " + scan.getFilter().toString());
        }
        catch (JSONException e) {
            e.printStackTrace();
        }
    }

    @Override
    public RegionScanner postScannerOpen(ObserverContext<RegionCoprocessorEnvironment> c, Scan scan, RegionScanner s) throws IOException {
        LOG.info("postScannerOpen :: " + scan.toString());
        return s;
    }

    @Override
    public boolean postScannerNext(ObserverContext<RegionCoprocessorEnvironment> c,
                                   InternalScanner s,
                                   List<Result> result,
                                   int limit,
                                   boolean hasNext)
            throws IOException {
        try {
            LOG.info(String.valueOf(hasNext));






            Table table = c.getEnvironment().getConnection().getTable(TableName.valueOf(INDEX_TABLE));


            String filterType="";
            String filterColumnFamily = "";
            String filterColumn ="";

            String filterValue = "";

            LOG.info("postScannerNext::filter string" + filter_string);


            if(families.length()>1 && columns.length()>1){
                return hasNext;
            }

            if( (tokens = isValueFilter(filter_string))!=null){
                if(tokens.get(0).equals("ValueFilter") && tokens.get(1).equals("EQUAL") && columns.get(0).toString().equals(INDEX_COLUMN)){
                     filterType = tokens.get(0);

                     filterValue = tokens.get(2);


                }
                else{
                    LOG.info(" postScannerNext:: Not using secndary index");
                    return hasNext;
                }
            }

            //SingleColumnValueFilter (cf, first_name, EQUAL, Kenny)
            else if((tokens = isSingleValueFilter(filter_string))!=null &&  tokens.get(3).equals("EQUAL")){
                if(tokens.get(0).equals("SingleColumnValueFilter")){
                    filterType = tokens.get(0);
                    filterColumnFamily = tokens.get(1);
                    filterColumn = tokens.get(2);
                    filterValue = tokens.get(4);

                }
                else{
                    LOG.info(" postScannerNext :: Not using secndary index");
                    return hasNext;
                }
            }
            else{
                LOG.info("postScannerNext:: Not using secndary index");
                return hasNext;
            }



            LOG.info("postScannerNext::prefix value :" + filterValue);

            Scan newscan = new Scan().withStartRow(Bytes.toBytes(filterValue));
            Filter prefixFilter = new PrefixFilter(Bytes.toBytes(filterValue));
            newscan.setFilter(prefixFilter);

            ResultScanner resultnew = table.getScanner(newscan);

            Table mainTable = c.getEnvironment().getConnection().getTable(TableName.valueOf(SOURCE_TABLE));
            int i = 0;

            LOG.info("postScannerNext::Result parameter size : " + result.size());

            String thisWorker = Bytes.toString(c.getEnvironment().getRegionInfo().getRegionName());

            if (!thisWorker.equals(workerRegion)) {
                return false;
            }




            for (Result r : resultnew) {
                byte[] row_key = r.getValue(Bytes.toBytes(CF), Bytes.toBytes(SOURCE_COLUMN));
                Get g = new Get(row_key);
                if(filterType.equals("ValueFilter")){
                    g.addColumn(Bytes.toBytes(CF), Bytes.toBytes(INDEX_COLUMN));
                }
                else if(filterType.equals("SingleColumnValueFilter")){
                    if(!columns.get(0).equals("ALL")){
                        for(int j=0;j<columns.length();j++){
                            g.addColumn(Bytes.toBytes(CF),Bytes.toBytes(columns.get(j).toString()));
                            LOG.info("postScannerNext::  columns.get(i)" +columns.get(j).toString());
                        }
                    }
                }
                else{
                    LOG.info("postScannerNext:: In Else");
                }
                Result mainResult = mainTable.get(g);
                result.add(mainResult);

                i++;

            }

            LOG.info("postScannerNext::Result parameter size : " + result.size());
            LOG.info("Loop run :" + i);

            return false;
        } catch (Exception e) {
            if (debug) LOG.info(e.getMessage());
        }
        return hasNext;

    }

    @Override
    public void postDelete(ObserverContext<RegionCoprocessorEnvironment> c, Delete delete, WALEdit edit, Durability durability) throws IOException {
        byte[] del_main_row_key = delete.getRow();
        Table table = c.getEnvironment().getConnection().getTable(TableName.valueOf(INDEX_TABLE));
        Scan scan = new Scan();
        if (debug) LOG.info("postDelete main_row_key ----> : " + Bytes.toString(del_main_row_key));
        Filter filter = new SingleColumnValueFilter(Bytes.toBytes(CF), Bytes.toBytes(SOURCE_COLUMN), CompareOperator.EQUAL, del_main_row_key);
        scan.setFilter(filter);
        ResultScanner result = table.getScanner(scan);
        for (Result r : result) {
            byte[] secondary_row_key = r.getRow();
            if (debug) LOG.info("postDelete secondary_row_key ----> : " + Bytes.toString(secondary_row_key));
            Delete secondaryDelete = new Delete(secondary_row_key);
            secondaryDelete.addColumn(Bytes.toBytes(CF), Bytes.toBytes(SOURCE_COLUMN));
            table.delete(secondaryDelete);

        }
        table.close();

    }


    public void printRows(ResultScanner resultScanner) {
        for (Result result : resultScanner) {
            printRow(result);
        }
    }

    public void printRow(Result result) {

        LOG.info(result.toString());

    }

    private ArrayList<String> isSingleValueFilter(String filter) {
        if(filter.isEmpty()){
            return null;
        }
        Pattern pattern = Pattern.compile("(SingleColumnValueFilter)\\s[(](\\w+),\\s(\\w+),\\s(\\w+),\\s(\\w+)[)]", Pattern.CASE_INSENSITIVE);
        Matcher matcher = pattern.matcher(filter);
        boolean matchFound = matcher.find();
        if (matchFound) {
            ArrayList<String> parsedOutput = new ArrayList<>();
            parsedOutput.add(0, matcher.group(1));
            parsedOutput.add(1, matcher.group(2));
            parsedOutput.add(2, matcher.group(3));
            parsedOutput.add(3, matcher.group(4));
            parsedOutput.add(4, matcher.group(5));
            return parsedOutput;

        } else {
            return null;
        }
    }

    private ArrayList<String> isValueFilter(String filter) {
        if(filter.isEmpty()){
            return null;
        }
        Pattern pattern = Pattern.compile("(ValueFilter)\\s[(](\\w+),\\s(\\w+)[)]", Pattern.CASE_INSENSITIVE);
        Matcher matcher = pattern.matcher(filter);
        boolean matchFound = matcher.find();
        if (matchFound) {
            ArrayList<String> parsedOutput = new ArrayList<>();
            parsedOutput.add(0, matcher.group(1));
            parsedOutput.add(1, matcher.group(2));
            parsedOutput.add(2, matcher.group(3));
            return parsedOutput;

        } else {
            System.out.println("No match found");
            return null;
        }
    }
}
