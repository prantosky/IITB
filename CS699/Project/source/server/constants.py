SL1_PREFIX = '10.130.153.'
SL2_PREFIX = '10.130.154.'
SL3_PREFIX = '10.130.155.'
CS101_PREFIX = '10.130.152.'
SL2_LAST_NODE = 132
SL1_LAST_NODE = 83
SL3_LAST_NODE = 44
CS101_LAST_NODE = 148
HEAD_NODE = 200

SL1_IPs = [SL1_PREFIX + str(i) for i in range(1, SL1_LAST_NODE + 1)]
SL2_IPs = [SL2_PREFIX + str(i) for i in range(1, SL2_LAST_NODE + 1)]
SL3_IPs = [SL3_PREFIX + str(i) for i in range(1, SL3_LAST_NODE + 1)]
CS101_IPs = [CS101_PREFIX + str(i) for i in range(1, CS101_LAST_NODE + 1)]

SL1_IPs.append(SL1_PREFIX + str(HEAD_NODE))
SL2_IPs.append(SL2_PREFIX + str(HEAD_NODE))
SL3_IPs.append(SL3_PREFIX + str(HEAD_NODE))
CS101_IPs.append(CS101_PREFIX + str(HEAD_NODE))