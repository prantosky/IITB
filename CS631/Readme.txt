Compile the java programs by using the commands

javac -cp `hbase classpath` SIC.java
javac -cp `hbase classpath` LoadCoProcessor.java 
javac -cp `hbase classpath` RemoveCoProcessor.java

Create the jar file for SIC class

Modify the path of SIC jar file and Secondary Index details in LoadCoProcessor and RemoveCoProcessor  

Run the program using below commands

java -cp `hbase classpath` LoadCoProcessor
	To load the coprocessor

java -cp `hbase classpath` RemoveCoProcessor
	To remove the coprocessor
	Note: Restart Hbase to reflect the changes.
