import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.hbase.*;
import org.apache.hadoop.hbase.client.*;
import org.apache.hadoop.hbase.io.compress.Compression;
import org.apache.hadoop.hbase.io.encoding.DataBlockEncoding;
import org.apache.hadoop.hbase.thrift.generated.ColumnDescriptor;
import org.apache.hadoop.hbase.util.Bytes;

import java.io.IOException;

public class LoadCoProcessor {
    public static void main(String args[]) {
        try {
        TableName tableName = TableName.valueOf("Sales");
            ColumnFamilyDescriptorBuilder columnDescBuilder = ColumnFamilyDescriptorBuilder
                    .newBuilder(Bytes.toBytes("cf"));


            String path = "file:////home/shailendra/softwares/hbase_data/SIC.jar";
        Configuration conf = HBaseConfiguration.create();
        Connection connection = null;

            connection = ConnectionFactory.createConnection(conf);

        Admin admin = connection.getAdmin();
        TableDescriptorBuilder tableDescBuilder = TableDescriptorBuilder.newBuilder(tableName);
            tableDescBuilder.setColumnFamily(columnDescBuilder.build());
            tableDescBuilder.setValue("COPROCESSOR$1", path + "|"
                + "SIC" + "|"
                + Coprocessor.PRIORITY_USER + "|arg1=Sales,arg2=cf,arg3=first_name,arg4=id");



            TableDescriptor tableDescriptor = tableDescBuilder.build();

            admin.modifyTable(tableDescriptor);
    }
        catch (IOException e) {
            e.printStackTrace();
        }
    }
}
