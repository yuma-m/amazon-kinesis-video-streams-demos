WORKSPACE="producer-c/producer-cloudwatch-integ"
GROOVY_SCRIPT_DIR="$WORKSPACE/jobs"
DAYS_TO_KEEP_LOGS=7
NUMBER_OF_LOGS=5
MAX_EXECUTION_PER_NODE=1

pipelineJob('executor') {
	description('Run producer and consumer')
	logRotator {
		daysToKeep(DAYS_TO_KEEP_LOGS)
		numToKeep(NUMBER_OF_LOGS)
	}	
	throttleConcurrentBuilds {
		maxPerNode(MAX_EXECUTION_PER_NODE)
	}
	definition {
        cps {
        	script(readFileFromWorkspace("${GROOVY_SCRIPT_DIR}/executor.groovy"))
            sandbox()
        }
    }
}