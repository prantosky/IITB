import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.hbase.*;
import org.apache.hadoop.hbase.client.*;
import org.apache.hadoop.hbase.io.compress.Compression;
import org.apache.hadoop.hbase.io.encoding.DataBlockEncoding;
import org.apache.hadoop.hbase.thrift.generated.ColumnDescriptor;
import org.apache.hadoop.hbase.util.Bytes;

import java.io.IOException;

public class RemoveCoProcessor {
    public static void main(String args[]) {
        try{

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
            tableDescBuilder.removeValue(Bytes.toBytes("COPROCESSOR$1"));



            TableDescriptor tableDescriptor = tableDescBuilder.build();

            admin.modifyTable(tableDescriptor);
            System.out.println("Removing coprocessor");
//            System.out.println("Stopping Hbase");
//            String[] cmd = new String[] {"/usr/bin/zsh", "-c", "/home/shailendra/softwares/hbase/bin/stop-hbase.sh"};
//            Process proc = new ProcessBuilder(cmd).start();
//            proc.waitFor();
//            System.out.println("Starting Hbase");
//            String[] cmd1 = new String[] {"/usr/bin/zsh", "-c", "/home/shailendra/softwares/hbase/bin/start-hbase.sh"};
//            Process proc1 = new ProcessBuilder(cmd1).start();
//            proc1.waitFor();

        }
        catch (IOException  e) {
            e.printStackTrace();
        }
    }
}
