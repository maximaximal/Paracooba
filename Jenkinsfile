pipeline {
    agent any
    options {
	buildDiscarder(logRotator(numToKeepStr: '10'))
    }

    stages {
	stage('Build') {
	    steps {
		ircNotify notifyOnStart:true, extraMessage: "Starting Build of ParaCuber."
		dir('paracuber') {
		    sh 'mkdir -p build/third_party/cadical-out/build/ && touch build/third_party/cadical-out/build/libcadical.a'
		    cmakeBuild buildType: 'Release', buildDir: 'build', cleanBuild: true, installation: 'InSearchPath', steps: [[withCmake: true]]
		}
	    }
	}
    }

    post {
        always {
            ircNotify()
        }
    }
}
