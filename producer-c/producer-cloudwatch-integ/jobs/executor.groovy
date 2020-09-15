import org.jenkinsci.plugins.workflow.steps.FlowInterruptedException

WORKSPACE_PRODUCER="producer-c/producer-cloudwatch-integ"
WORKSPACE_CONSUMER="consumer-java/aws-kinesis-video-producer-sdk-canary-consumer"
GIT_URL="https://github.com/aws-samples/amazon-kinesis-video-streams-demos.git"
BRANCH="refs/heads/producer"

JAR_FILES=""
CLASSPATH_VALUES=""

def buildProducer() {
  sh  """
    sudo git clean -fdx
    cd $WORKSPACE_PRODUCER && 
    mkdir build && 
    cd build && 
    sudo cmake .. && 
    sudo make 
  """
}

def buildConsumer() {
  sh '''
    JAVA_HOME='/opt/jdk-13.0.1'
    PATH="$JAVA_HOME/bin:$PATH"
    export PATH
    M2_HOME='/opt/apache-maven-3.6.3'
    PATH="$M2_HOME/bin:$PATH"
    export PATH
    cd $WORKSPACE/consumer-java/aws-kinesis-video-producer-sdk-canary-consumer
    mvn package
    # Create a temporary filename in /tmp directory
    JAR_FILES=$(mktemp)
    # Create classpath string of dependencies from the local repository to a file
    mvn -Dmdep.outputFile=$JAR_FILES dependency:build-classpath
    CLASSPATH_VALUES=$(cat ${JAR_FILES})
  '''
}

pipeline {
   agent any
   stages {
      stage('Checkout') {
         steps {
              echo env.NODE_NAME
              checkout([
                        scm: [
                            $class: 'GitSCM', 
                            branches: [[name: BRANCH]],
                            userRemoteConfigs: [[url: GIT_URL]]]
                        ])
         }
      }
      
      stage('Build') {
          steps {
                echo 'Build Consumer'
                buildProducer()
                echo 'Build Consumer'
                buildConsumer()
          }
      }
      
      stage('Run') {
          steps {
              echo 'Run'
          }
      }
      
      stage('CleanUp') {
          steps {
              echo 'CleanUp'
          }
      }
   }
}
      
